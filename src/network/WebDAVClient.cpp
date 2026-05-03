#include "network/WebDAVClient.h"

#include <HTTPClient.h>
#include <Logging.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <XmlParserUtils.h>
#include <base64.h>
#include <expat.h>

#include <cstring>
#include <memory>

#include "util/UrlUtils.h"

namespace {

// Get local element name, ignoring namespace prefix
const char* localName(const char* name) {
  const char* colon = strrchr(name, ':');
  return colon ? colon + 1 : name;
}

// Simple URL-decode for std::string
std::string urlDecode(const std::string& src) {
  std::string decoded;
  decoded.reserve(src.length());
  for (size_t i = 0; i < src.length(); ++i) {
    if (src[i] == '%' && i + 2 < src.length()) {
      int hex = 0;
      for (int j = 1; j <= 2; ++j) {
        char c = src[i + j];
        hex <<= 4;
        if (c >= '0' && c <= '9')
          hex |= c - '0';
        else if (c >= 'A' && c <= 'F')
          hex |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
          hex |= c - 'a' + 10;
      }
      decoded += static_cast<char>(hex);
      i += 2;
    } else if (src[i] == '+') {
      decoded += ' ';
    } else {
      decoded += src[i];
    }
  }
  return decoded;
}

// Extract the last path component for display name
std::string hrefToName(const std::string& href) {
  std::string decoded = urlDecode(href);
  // Remove trailing slash for directories
  if (!decoded.empty() && decoded.back() == '/') {
    decoded.pop_back();
  }
  size_t pos = decoded.find_last_of('/');
  return (pos != std::string::npos) ? decoded.substr(pos + 1) : decoded;
}

// --- Expat-based streaming PROPFIND parser ---

class PropfindParser final : public Print {
 public:
  PropfindParser();
  ~PropfindParser();
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;
  void flush();
  bool error() const { return errorOccurred; }

  std::vector<WebDAVItem> items;
  bool truncated = false;

 private:
  static constexpr size_t MAX_ITEMS = 200;
  static void XMLCALL startElement(void*, const XML_Char*, const XML_Char**);
  static void XMLCALL endElement(void*, const XML_Char*);
  static void XMLCALL characterData(void*, const XML_Char*, int);

  XML_Parser parser = nullptr;
  WebDAVItem current;
  std::string currentText;
  bool inHref = false, inResourceType = false, inContentLength = false;
  bool currentIsCollection = false;
  bool firstResponseSeen = false, skipFirst = true;
  bool errorOccurred = false;
};

PropfindParser::PropfindParser() {
  parser = XML_ParserCreate(nullptr);
  if (!parser) {
    errorOccurred = true;
    LOG_DBG("WEBDAV", "Couldn't allocate memory for parser");
  } else {
    items.reserve(MAX_ITEMS);
  }
}

PropfindParser::~PropfindParser() { destroyXmlParser(parser); }

size_t PropfindParser::write(uint8_t c) { return write(&c, 1); }

size_t PropfindParser::write(const uint8_t* xmlData, size_t length) {
  if (errorOccurred) return length;

  XML_SetUserData(parser, this);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);

  const char* currentPos = reinterpret_cast<const char*>(xmlData);
  size_t remaining = length;
  constexpr size_t chunkSize = 1024;

  while (remaining > 0) {
    void* const buf = XML_GetBuffer(parser, chunkSize);
    if (!buf) {
      errorOccurred = true;
      LOG_DBG("WEBDAV", "Couldn't allocate memory for buffer");
      destroyXmlParser(parser);
      return length;
    }

    const size_t toRead = remaining < chunkSize ? remaining : chunkSize;
    memcpy(buf, currentPos, toRead);

    if (XML_ParseBuffer(parser, static_cast<int>(toRead), 0) == XML_STATUS_ERROR) {
      errorOccurred = true;
      LOG_DBG("WEBDAV", "Parse error at line %lu: %s", XML_GetCurrentLineNumber(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
      destroyXmlParser(parser);
      return length;
    }
    currentPos += toRead;
    remaining -= toRead;
  }
  return length;
}

void PropfindParser::flush() {
  if (XML_Parse(parser, nullptr, 0, XML_TRUE) != XML_STATUS_OK) {
    errorOccurred = true;
    destroyXmlParser(parser);
  }
}

void XMLCALL PropfindParser::startElement(void* userData, const XML_Char* name, const XML_Char** /*atts*/) {
  auto* self = static_cast<PropfindParser*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    if (!self->firstResponseSeen) {
      self->firstResponseSeen = true;
      self->skipFirst = true;
    } else {
      self->skipFirst = false;
    }
    self->current = WebDAVItem{};
    self->currentIsCollection = false;
  } else if (strcmp(local, "href") == 0) {
    self->inHref = true;
    self->currentText.clear();
  } else if (strcmp(local, "resourcetype") == 0) {
    self->inResourceType = true;
  } else if (strcmp(local, "collection") == 0) {
    if (self->inResourceType) {
      self->currentIsCollection = true;
    }
  } else if (strcmp(local, "getcontentlength") == 0) {
    self->inContentLength = true;
    self->currentText.clear();
  }
}

void XMLCALL PropfindParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<PropfindParser*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    if (!self->skipFirst && !self->current.href.empty()) {
      if (self->items.size() < MAX_ITEMS) {
        self->current.isDirectory = self->currentIsCollection;
        self->current.name = hrefToName(self->current.href);
        if (self->current.name.empty()) {
          self->current.name = self->current.href;
        }
        self->items.push_back(std::move(self->current));
      } else {
        self->truncated = true;
      }
    }
  } else if (strcmp(local, "href") == 0) {
    self->inHref = false;
    if (!self->skipFirst) {
      self->current.href = self->currentText;
    }
  } else if (strcmp(local, "resourcetype") == 0) {
    self->inResourceType = false;
  } else if (strcmp(local, "getcontentlength") == 0) {
    self->inContentLength = false;
    if (!self->skipFirst) {
      self->current.size = strtoull(self->currentText.c_str(), nullptr, 10);
    }
  }
}

void XMLCALL PropfindParser::characterData(void* userData, const XML_Char* s, int len) {
  auto* self = static_cast<PropfindParser*>(userData);
  if (self->inHref || self->inContentLength) {
    self->currentText.append(s, len);
  }
}

class PropfindStream final : public Stream {
 public:
  explicit PropfindStream(PropfindParser& p) : parser(p) {}
  int available() override { return 0; }
  int peek() override { abort(); }
  int read() override { abort(); }
  size_t write(uint8_t c) override { return parser.write(c); }
  size_t write(const uint8_t* b, size_t n) override { return parser.write(b, n); }
  ~PropfindStream() override { parser.flush(); }

 private:
  PropfindParser& parser;
};

}  // namespace

WebDAVError WebDAVClient::propfind(const std::string& url, std::vector<WebDAVItem>& out,
                                   const std::string& username, const std::string& password,
                                   bool* truncated) {
  out.clear();
  if (truncated) *truncated = false;

  std::unique_ptr<NetworkClient> client;
  if (UrlUtils::isHttpsUrl(url)) {
    auto* secureClient = new NetworkClientSecure();
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new NetworkClient());
  }

  HTTPClient http;
  http.begin(*client, url.c_str());
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
    LOG_ERR("WEBDAV", "PROPFIND failed: %d", httpCode);
    http.end();
    if (httpCode == 401 || httpCode == 403) {
      return WebDAVError::AUTH_ERROR;
    }
    return WebDAVError::HTTP_ERROR;
  }

  PropfindParser parser;
  {
    PropfindStream stream{parser};
    http.writeToStream(&stream);
  }  // ~PropfindStream() calls parser.flush()
  http.end();

  if (parser.error()) return WebDAVError::PARSE_ERROR;
  if (truncated) *truncated = parser.truncated;
  out = std::move(parser.items);
  LOG_DBG("WEBDAV", "PROPFIND %zu items%s", out.size(),
          parser.truncated ? " (truncated at MAX_ITEMS)" : "");
  return WebDAVError::OK;
}

WebDAVError WebDAVClient::downloadFile(const std::string& url, const std::string& destPath,
                                       HttpDownloader::ProgressCallback progress, const std::string& username,
                                       const std::string& password) {
  auto result = HttpDownloader::downloadToFile(url, destPath, progress, username, password);
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
