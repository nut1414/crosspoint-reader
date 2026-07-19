#include "network/WebDAVClient.h"

#include <HTTPClient.h>
#include <Logging.h>
#include <Memory.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <base64.h>

#include <cstring>
#include <memory>
#include <utility>

#include "network/WebDAVPropfindParser.h"
#include "network/WebDAVUrl.h"
#include "util/UrlUtils.h"

namespace {

class PropfindStream final : public Stream {
 public:
  explicit PropfindStream(WebDAVPropfindParser& p) : parser(p) {}
  int available() override { return 0; }
  int peek() override { abort(); }
  int read() override { abort(); }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    return parser.feed(reinterpret_cast<const char*>(b), n) ? n : 0;
  }
  ~PropfindStream() override { parser.finish(); }

 private:
  WebDAVPropfindParser& parser;
};

}  // namespace

WebDAVError WebDAVClient::propfind(const std::string& url, WebDAVItemList& out,
                                   const std::string& username, const std::string& password,
                                   bool* truncated, const std::string& baseUrl) {
  // Copy the URL before clearing output so callers cannot invalidate an aliased
  // item href. The browser passes separate fields, but the public API need not.
  const std::string requestUrl = UrlUtils::ensureProtocol(url);
  out.clear();
  if (truncated) *truncated = false;

  // Allocate reusable listing storage before HTTP/TLS consumes and fragments
  // the heap. Allocation failure is recoverable instead of reaching vector's
  // fatal uncaught allocation path.
  WebDAVPropfindParser parser{out};
  if (parser.outOfMemory()) {
    LOG_ERR("WEBDAV", "OOM allocating WebDAV directory listing");
    return WebDAVError::OUT_OF_MEMORY;
  }
  if (parser.error()) {
    LOG_ERR("WEBDAV", "Failed to create PROPFIND parser");
    return WebDAVError::PARSE_ERROR;
  }

  std::unique_ptr<NetworkClient> client;
  if (requestUrl.rfind("https://", 0) == 0) {
    auto secureClient = makeUniqueNoThrow<NetworkClientSecure>();
    if (!secureClient) {
      LOG_ERR("WEBDAV", "OOM creating TLS client");
      return WebDAVError::NETWORK_ERROR;
    }
    secureClient->setInsecure();
    client = std::move(secureClient);
  } else {
    client = makeUniqueNoThrow<NetworkClient>();
    if (!client) {
      LOG_ERR("WEBDAV", "OOM creating network client");
      return WebDAVError::NETWORK_ERROR;
    }
  }

  HTTPClient http;
  if (!http.begin(*client, requestUrl.c_str())) {
    LOG_ERR("WEBDAV", "PROPFIND failed to initialize URL: %s", requestUrl.c_str());
    return WebDAVError::NETWORK_ERROR;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  http.addHeader("Content-Type", "application/xml");
  http.addHeader("Depth", "1");

  if (!username.empty() && !password.empty()) {
    std::string credentials = username + ":" + password;
    String encoded = base64::encode(credentials.c_str());
    http.addHeader("Authorization", "Basic " + encoded);
  }

  // Send minimal PROPFIND body
  const char* propfindBody =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<propfind xmlns=\"DAV:\">"
      "<prop>"
      "<resourcetype/>"
      "<getcontentlength/>"
      "</prop>"
      "</propfind>";

  const int httpCode = http.sendRequest("PROPFIND", (uint8_t*)propfindBody, strlen(propfindBody));
  if (httpCode != HTTP_CODE_MULTI_STATUS) {
    String status = httpCode < 0 ? HTTPClient::errorToString(httpCode) : String("HTTP status");
    LOG_ERR("WEBDAV", "PROPFIND failed: %d (%s) url=%s", httpCode, status.c_str(), requestUrl.c_str());
    http.end();
    if (httpCode < 0) {
      return WebDAVError::NETWORK_ERROR;
    }
    if (httpCode == 401 || httpCode == 403) {
      return WebDAVError::AUTH_ERROR;
    }
    return WebDAVError::HTTP_ERROR;
  }

  int writeResult = 0;
  {
    PropfindStream stream{parser};
    writeResult = http.writeToStream(&stream);
  }  // ~PropfindStream() calls parser.finish()
  http.end();

  // A parser failure deliberately produces a short stream write. Classify it
  // before HTTPClient's resulting stream error so malformed XML and parser OOM
  // are not mislabeled as network failures.
  if (parser.error()) {
    out.clear();
    if (parser.outOfMemory()) {
      LOG_ERR("WEBDAV", "OOM parsing PROPFIND response");
      return WebDAVError::OUT_OF_MEMORY;
    }
    LOG_ERR("WEBDAV", "Invalid PROPFIND response");
    return WebDAVError::PARSE_ERROR;
  }
  if (writeResult < 0) {
    out.clear();
    LOG_ERR("WEBDAV", "PROPFIND read failed: %d (%s) url=%s", writeResult,
            HTTPClient::errorToString(writeResult).c_str(), requestUrl.c_str());
    return WebDAVError::NETWORK_ERROR;
  }
  if (truncated) *truncated = parser.truncated;
  WebDAVUrl::resolveItems(baseUrl.empty() ? requestUrl : baseUrl, requestUrl, out);
  LOG_DBG("WEBDAV", "PROPFIND %zu items%s", out.size(),
          parser.truncated ? " (truncated at MAX_ITEMS)" : "");
  return WebDAVError::OK;
}

WebDAVError WebDAVClient::downloadFile(const std::string& url, const std::string& destPath,
                                       HttpDownloader::ProgressCallback progress, const std::string& username,
                                       const std::string& password) {
  auto result = HttpDownloader::downloadToFile(url, destPath, progress, nullptr, username, password);
  switch (result) {
    case HttpDownloader::OK:
      return WebDAVError::OK;
    case HttpDownloader::HTTP_ERROR:
      return WebDAVError::HTTP_ERROR;
    case HttpDownloader::FILE_ERROR:
      return WebDAVError::NETWORK_ERROR;
    case HttpDownloader::ABORTED:
      return WebDAVError::NETWORK_ERROR;
    default:
      return WebDAVError::NETWORK_ERROR;
  }
}
