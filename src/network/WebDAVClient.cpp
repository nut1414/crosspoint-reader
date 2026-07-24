#include "network/WebDAVClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <base64.h>

#include <cstring>
#include <memory>
#include <utility>

#include "network/HttpChunkDecoder.h"
#include "network/WebDAVPropfindParser.h"
#include "network/WebDAVResponseEndDetector.h"
#include "network/WebDAVUrl.h"
#include "util/UrlUtils.h"

namespace {

constexpr size_t HTTPS_MAX_ITEMS = 64;
constexpr int32_t WEBDAV_CONNECT_TIMEOUT_MS = 20000;
constexpr uint16_t WEBDAV_RESPONSE_TIMEOUT_MS = 45000;
constexpr unsigned long TLS_HANDSHAKE_TIMEOUT_SECONDS = 30;
constexpr size_t REQUEST_ATTEMPTS = 3;
constexpr uint32_t REQUEST_RETRY_DELAY_MS = 250;
constexpr size_t NETWORK_READ_CHUNK = 512;
constexpr size_t RESPONSE_READ_CHUNK = 1024;
constexpr size_t MAX_RESPONSE_BYTES = 1024 * 1024;
constexpr const char* RESPONSE_TEMP_PATH = "/.crosspoint/webdav_propfind.tmp";

enum class SpoolResult { OK, NETWORK_ERROR, STORAGE_ERROR, TOO_LARGE, INVALID_RESPONSE };

struct SpoolAttemptResult {
  WebDAVError error;
  bool retryable;
};

class WebDAVHTTPClient final : public HTTPClient {
 public:
  bool responseIsChunked() const noexcept { return _transferEncoding == HTTPC_TE_CHUNKED; }
};

bool isRetryableTransportError(const int error) {
  switch (error) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
    case HTTPC_ERROR_SEND_HEADER_FAILED:
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
    case HTTPC_ERROR_NOT_CONNECTED:
    case HTTPC_ERROR_CONNECTION_LOST:
    case HTTPC_ERROR_NO_HTTP_SERVER:
    case HTTPC_ERROR_READ_TIMEOUT:
      return true;
    default:
      return false;
  }
}

bool isRetryableStatus(const int status) {
  return status == HTTP_CODE_REQUEST_TIMEOUT || status == HTTP_CODE_BAD_GATEWAY ||
         status == HTTP_CODE_SERVICE_UNAVAILABLE || status == HTTP_CODE_GATEWAY_TIMEOUT;
}

SpoolResult writeResponseBytes(HalFile& output, const uint8_t* data, const size_t size, size_t& total) {
  if (size > MAX_RESPONSE_BYTES - total) return SpoolResult::TOO_LARGE;
  if (output.write(data, size) != size) return SpoolResult::STORAGE_ERROR;
  total += size;
  return SpoolResult::OK;
}

SpoolResult spoolResponseBody(WebDAVHTTPClient& http, HalFile& output) {
  NetworkClient* const response = http.getStreamPtr();
  if (!response) return SpoolResult::NETWORK_ERROR;

  const bool chunked = http.responseIsChunked();
  int remaining = chunked ? -1 : http.getSize();
  if (!chunked && remaining == 0) return SpoolResult::INVALID_RESPONSE;
  LOG_DBG("WEBDAV", "PROPFIND response framing=%s length=%d", chunked ? "chunked" : "identity",
          http.getSize());

  size_t total = 0;
  uint32_t lastDataAt = millis();
  uint8_t buffer[NETWORK_READ_CHUNK];
  HttpChunkDecoder chunkDecoder;
  WebDAVResponseEndDetector responseEndDetector;

  while (true) {
    const int available = response->available();
    if (available > 0) {
      size_t toRead = static_cast<size_t>(available);
      if (toRead > sizeof(buffer)) toRead = sizeof(buffer);
      if (remaining > 0 && toRead > static_cast<size_t>(remaining)) {
        toRead = static_cast<size_t>(remaining);
      }

      const int bytesRead = response->read(buffer, toRead);
      if (bytesRead < 0) return SpoolResult::NETWORK_ERROR;
      if (bytesRead > 0) {
        lastDataAt = millis();
        if (chunked) {
          size_t decodedSize = 0;
          bool complete = false;
          for (int i = 0; i < bytesRead; ++i) {
            const uint8_t byte = buffer[i];
            const auto result = chunkDecoder.consume(byte);
            if (result == HttpChunkDecoder::Result::ERROR) return SpoolResult::INVALID_RESPONSE;
            if (result == HttpChunkDecoder::Result::DATA) buffer[decodedSize++] = byte;
            if (result == HttpChunkDecoder::Result::COMPLETE) {
              complete = true;
              break;
            }
          }
          const SpoolResult writeResult = writeResponseBytes(output, buffer, decodedSize, total);
          if (writeResult != SpoolResult::OK) return writeResult;
          if (complete) return total > 0 ? SpoolResult::OK : SpoolResult::INVALID_RESPONSE;
        } else {
          size_t bodySize = static_cast<size_t>(bytesRead);
          bool documentComplete = false;
          if (remaining < 0) {
            for (int i = 0; i < bytesRead; ++i) {
              responseEndDetector.consume(buffer[i]);
              if (responseEndDetector.complete()) {
                bodySize = static_cast<size_t>(i + 1);
                documentComplete = true;
                break;
              }
            }
          }
          const SpoolResult writeResult =
              writeResponseBytes(output, buffer, bodySize, total);
          if (writeResult != SpoolResult::OK) return writeResult;
          if (documentComplete) return SpoolResult::OK;
          if (remaining > 0) remaining -= bytesRead;
          if (remaining == 0) return total > 0 ? SpoolResult::OK : SpoolResult::INVALID_RESPONSE;
        }
        continue;
      }
    }

    if (!response->connected()) {
      return SpoolResult::NETWORK_ERROR;
    }
    if (millis() - lastDataAt > WEBDAV_RESPONSE_TIMEOUT_MS) return SpoolResult::NETWORK_ERROR;
    delay(10);
  }
}

SpoolAttemptResult spoolPropfindResponse(const std::string& requestUrl, const bool useTls,
                                         const std::string& username, const std::string& password) {
  if (Storage.exists(RESPONSE_TEMP_PATH)) Storage.remove(RESPONSE_TEMP_PATH);

  std::unique_ptr<NetworkClient> client;
  NetworkClientSecure* secureClientForError = nullptr;
  if (useTls) {
    auto secureClient = makeUniqueNoThrow<NetworkClientSecure>();
    if (!secureClient) {
      LOG_ERR("WEBDAV", "OOM creating TLS client");
      return {WebDAVError::OUT_OF_MEMORY, false};
    }
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_SECONDS);
    secureClientForError = secureClient.get();
    client = std::move(secureClient);
  } else {
    client = makeUniqueNoThrow<NetworkClient>();
    if (!client) {
      LOG_ERR("WEBDAV", "OOM creating network client");
      return {WebDAVError::OUT_OF_MEMORY, false};
    }
  }

  WebDAVHTTPClient http;
  if (!http.begin(*client, requestUrl.c_str())) {
    LOG_ERR("WEBDAV", "PROPFIND failed to initialize URL: %s", requestUrl.c_str());
    return {WebDAVError::NETWORK_ERROR, false};
  }
  http.setConnectTimeout(WEBDAV_CONNECT_TIMEOUT_MS);
  http.setTimeout(WEBDAV_RESPONSE_TIMEOUT_MS);
  http.setReuse(false);
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  http.addHeader("Content-Type", "application/xml");
  http.addHeader("Depth", "1");

  if (!username.empty() && !password.empty()) {
    const std::string credentials = username + ":" + password;
    const String encoded = base64::encode(credentials.c_str());
    http.addHeader("Authorization", "Basic " + encoded);
  }

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
    const String status = httpCode < 0 ? HTTPClient::errorToString(httpCode) : String("HTTP status");
    LOG_ERR("WEBDAV", "PROPFIND failed: %d (%s) url=%s", httpCode, status.c_str(), requestUrl.c_str());
    if (httpCode < 0 && secureClientForError) {
      char tlsError[128]{};
      const int tlsErrorCode = secureClientForError->lastError(tlsError, sizeof(tlsError));
      if (tlsErrorCode != 0) {
        LOG_ERR("WEBDAV", "TLS failure: %d (%s)", tlsErrorCode, tlsError);
      }
    }
    http.end();
    if (httpCode == HTTPC_ERROR_TOO_LESS_RAM) return {WebDAVError::OUT_OF_MEMORY, false};
    if (httpCode < 0) return {WebDAVError::NETWORK_ERROR, isRetryableTransportError(httpCode)};
    if (httpCode == 401 || httpCode == 403) return {WebDAVError::AUTH_ERROR, false};
    return {WebDAVError::HTTP_ERROR, isRetryableStatus(httpCode)};
  }

  if (http.getSize() > static_cast<int>(MAX_RESPONSE_BYTES)) {
    LOG_ERR("WEBDAV", "PROPFIND response exceeds %zu bytes", MAX_RESPONSE_BYTES);
    http.end();
    return {WebDAVError::HTTP_ERROR, false};
  }

  HalFile responseFile;
  if (!Storage.openFileForWrite("WEBDAV", RESPONSE_TEMP_PATH, responseFile)) {
    LOG_ERR("WEBDAV", "Failed to create temporary PROPFIND response file");
    http.end();
    return {WebDAVError::HTTP_ERROR, false};
  }

  const SpoolResult spoolResult = spoolResponseBody(http, responseFile);
  const size_t responseSize = responseFile.size();
  const bool closeSucceeded = responseFile.close();
  http.end();

  if (spoolResult == SpoolResult::TOO_LARGE) {
    LOG_ERR("WEBDAV", "PROPFIND response exceeded %zu bytes", MAX_RESPONSE_BYTES);
    Storage.remove(RESPONSE_TEMP_PATH);
    return {WebDAVError::HTTP_ERROR, false};
  }
  if (spoolResult == SpoolResult::NETWORK_ERROR) {
    Storage.remove(RESPONSE_TEMP_PATH);
    LOG_ERR("WEBDAV", "PROPFIND response read failed after %zu bytes: %s", responseSize, requestUrl.c_str());
    return {WebDAVError::NETWORK_ERROR, true};
  }
  if (spoolResult == SpoolResult::INVALID_RESPONSE) {
    LOG_ERR("WEBDAV", "Invalid HTTP framing in PROPFIND response");
    Storage.remove(RESPONSE_TEMP_PATH);
    return {WebDAVError::HTTP_ERROR, false};
  }
  if (spoolResult == SpoolResult::STORAGE_ERROR || !closeSucceeded || responseSize == 0) {
    LOG_ERR("WEBDAV", "Failed to store PROPFIND response");
    Storage.remove(RESPONSE_TEMP_PATH);
    return {WebDAVError::HTTP_ERROR, false};
  }
  return {WebDAVError::OK, false};
}

}  // namespace

WebDAVError WebDAVClient::propfind(const std::string& url, WebDAVItemList& out,
                                   const std::string& username, const std::string& password,
                                   bool* truncated, const std::string& baseUrl) {
  // Copy the URL before clearing output so callers cannot invalidate an aliased
  // item href. The browser passes separate fields, but the public API need not.
  const std::string requestUrl = UrlUtils::ensureProtocol(url);
  const bool useTls = requestUrl.rfind("https://", 0) == 0;
  // The previous directory buffer is intentionally not retained across
  // requests: mbedTLS needs that contiguous heap for its RSA handshake.
  out.releaseStorage();
  if (truncated) *truncated = false;

  WebDAVError requestError = WebDAVError::NETWORK_ERROR;
  for (size_t attempt = 0; attempt < REQUEST_ATTEMPTS; ++attempt) {
    const SpoolAttemptResult result = spoolPropfindResponse(requestUrl, useTls, username, password);
    requestError = result.error;
    if (requestError == WebDAVError::OK) break;
    if (!result.retryable || attempt + 1 == REQUEST_ATTEMPTS) return requestError;
    LOG_INF("WEBDAV", "Retrying PROPFIND after transient failure (attempt %zu/%zu)", attempt + 2,
            REQUEST_ATTEMPTS);
    delay(REQUEST_RETRY_DELAY_MS);
  }
  if (requestError != WebDAVError::OK) return requestError;

  HalFile responseInput;
  if (!Storage.openFileForRead("WEBDAV", RESPONSE_TEMP_PATH, responseInput)) {
    LOG_ERR("WEBDAV", "Failed to reopen temporary PROPFIND response file");
    Storage.remove(RESPONSE_TEMP_PATH);
    return WebDAVError::HTTP_ERROR;
  }
  const size_t storedResponseSize = responseInput.size();

  auto readBuffer = makeUniqueNoThrow<char[]>(RESPONSE_READ_CHUNK);
  if (!readBuffer) {
    LOG_ERR("WEBDAV", "OOM allocating PROPFIND read buffer");
    responseInput.close();
    Storage.remove(RESPONSE_TEMP_PATH);
    return WebDAVError::OUT_OF_MEMORY;
  }

  WebDAVPropfindParser parser{out, useTls ? HTTPS_MAX_ITEMS : WebDAVItemList::MAX_ITEMS};
  bool responseReadFailed = false;
  if (!parser.error()) {
    while (true) {
      const int bytesRead = responseInput.read(readBuffer.get(), RESPONSE_READ_CHUNK);
      if (bytesRead < 0) {
        LOG_ERR("WEBDAV", "Failed reading temporary PROPFIND response");
        responseReadFailed = true;
        break;
      }
      if (bytesRead == 0) {
        parser.finish();
        break;
      }
      if (!parser.feed(readBuffer.get(), static_cast<size_t>(bytesRead))) break;
    }
  }
  responseInput.close();
  Storage.remove(RESPONSE_TEMP_PATH);

  if (responseReadFailed) {
    out.releaseStorage();
    return WebDAVError::HTTP_ERROR;
  }

  if (parser.error()) {
    out.releaseStorage();
    if (parser.outOfMemory()) {
      LOG_ERR("WEBDAV", "OOM parsing PROPFIND response");
      return WebDAVError::OUT_OF_MEMORY;
    }
    const XML_Error xmlError = parser.xmlError();
    const char* const errorText = XML_ErrorString(xmlError);
    LOG_ERR("WEBDAV", "Invalid PROPFIND response: %s (%d) at byte %lld of %zu",
            errorText ? errorText : "unknown XML error", static_cast<int>(xmlError),
            static_cast<long long>(parser.xmlErrorOffset()), storedResponseSize);
    return WebDAVError::PARSE_ERROR;
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
