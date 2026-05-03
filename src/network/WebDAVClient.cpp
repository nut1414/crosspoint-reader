#include "network/WebDAVClient.h"

#include <HTTPClient.h>
#include <Logging.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <StreamString.h>
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

// --- Expat-based PROPFIND parser ---

struct PropfindParserState {
  std::vector<WebDAVItem> items;
  WebDAVItem current;
  std::string currentText;
  bool inHref = false;
  bool inResourceType = false;
  bool inContentLength = false;
  bool currentIsCollection = false;
  bool skipFirst = true;  // Skip the first response (the directory itself)
  bool firstResponseSeen = false;
  bool errorOccured = false;
  XML_Parser parser = nullptr;
};

void XMLCALL propfindStartElement(void* userData, const XML_Char* name, const XML_Char** /*atts*/) {
  auto* state = static_cast<PropfindParserState*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    if (!state->firstResponseSeen) {
      state->firstResponseSeen = true;
      state->skipFirst = true;
    } else {
      state->skipFirst = false;
    }
    state->current = WebDAVItem{};
    state->currentIsCollection = false;
  } else if (strcmp(local, "href") == 0) {
    state->inHref = true;
    state->currentText.clear();
  } else if (strcmp(local, "resourcetype") == 0) {
    state->inResourceType = true;
  } else if (strcmp(local, "collection") == 0) {
    if (state->inResourceType) {
      state->currentIsCollection = true;
    }
  } else if (strcmp(local, "getcontentlength") == 0) {
    state->inContentLength = true;
    state->currentText.clear();
  }
}

void XMLCALL propfindEndElement(void* userData, const XML_Char* name) {
  auto* state = static_cast<PropfindParserState*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    if (!state->skipFirst && !state->current.href.empty()) {
      state->current.isDirectory = state->currentIsCollection;
      state->current.name = hrefToName(state->current.href);
      if (state->current.name.empty()) {
        // Fallback: if name is empty after decoding, use the raw href
        state->current.name = state->current.href;
      }
      state->items.push_back(state->current);
    }
  } else if (strcmp(local, "href") == 0) {
    state->inHref = false;
    if (!state->skipFirst) {
      state->current.href = state->currentText;
    }
  } else if (strcmp(local, "resourcetype") == 0) {
    state->inResourceType = false;
  } else if (strcmp(local, "getcontentlength") == 0) {
    state->inContentLength = false;
    if (!state->skipFirst) {
      state->current.size = strtoull(state->currentText.c_str(), nullptr, 10);
    }
  }
}

void XMLCALL propfindCharacterData(void* userData, const XML_Char* s, int len) {
  auto* state = static_cast<PropfindParserState*>(userData);
  if (state->inHref || state->inContentLength) {
    state->currentText.append(s, len);
  }
}

}  // namespace

WebDAVError WebDAVClient::propfind(const std::string& url, std::vector<WebDAVItem>& out,
                                   const std::string& username, const std::string& password) {
  out.clear();

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

  // Read the full response body
  StreamString responseStream;
  http.writeToStream(&responseStream);
  http.end();

  std::string xmlBody(responseStream.c_str(), responseStream.length());
  responseStream.clear();  // free first copy before parser allocates buffers
  if (xmlBody.empty()) {
    LOG_ERR("WEBDAV", "PROPFIND returned empty body");
    return WebDAVError::PARSE_ERROR;
  }

  // Parse XML with Expat
  PropfindParserState state;
  state.parser = XML_ParserCreate(nullptr);
  if (!state.parser) {
    LOG_ERR("WEBDAV", "Failed to create XML parser");
    return WebDAVError::PARSE_ERROR;
  }

  XML_SetUserData(state.parser, &state);
  XML_SetElementHandler(state.parser, propfindStartElement, propfindEndElement);
  XML_SetCharacterDataHandler(state.parser, propfindCharacterData);

  if (XML_Parse(state.parser, xmlBody.c_str(), static_cast<int>(xmlBody.size()), XML_TRUE) != XML_STATUS_OK) {
    LOG_ERR("WEBDAV", "XML parse error at line %lu: %s", XML_GetCurrentLineNumber(state.parser),
            XML_ErrorString(XML_GetErrorCode(state.parser)));
    destroyXmlParser(state.parser);
    return WebDAVError::PARSE_ERROR;
  }

  destroyXmlParser(state.parser);

  if (state.errorOccured) {
    return WebDAVError::PARSE_ERROR;
  }

  out = std::move(state.items);
  LOG_DBG("WEBDAV", "PROPFIND returned %zu items", out.size());
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
