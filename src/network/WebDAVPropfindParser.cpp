#include "network/WebDAVPropfindParser.h"

#include <XmlParserUtils.h>

#include <cstdlib>
#include <cstring>
#include <utility>

namespace {

const char* localName(const char* name) {
  const char* colon = strrchr(name, ':');
  return colon ? colon + 1 : name;
}

std::string urlDecode(const std::string& src) {
  std::string decoded;
  decoded.reserve(src.length());
  for (size_t i = 0; i < src.length(); ++i) {
    if (src[i] == '%' && i + 2 < src.length()) {
      int hex = 0;
      bool valid = true;
      for (int j = 1; j <= 2; ++j) {
        char c = src[i + j];
        hex <<= 4;
        if (c >= '0' && c <= '9')
          hex |= c - '0';
        else if (c >= 'A' && c <= 'F')
          hex |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
          hex |= c - 'a' + 10;
        else
          valid = false;
      }
      if (valid) {
        decoded += static_cast<char>(hex);
        i += 2;
      } else {
        decoded += src[i];
      }
    } else {
      decoded += src[i];
    }
  }
  return decoded;
}

std::string hrefToName(const std::string& href) {
  std::string decoded = urlDecode(href);
  if (!decoded.empty() && decoded.back() == '/') {
    decoded.pop_back();
  }
  size_t pos = decoded.find_last_of('/');
  return (pos != std::string::npos) ? decoded.substr(pos + 1) : decoded;
}

}  // namespace

WebDAVPropfindParser::WebDAVPropfindParser(WebDAVItemList& output) : items(output) {
  items.clear();
  if (!items.ensureStorage()) {
    allocationFailed = true;
    errorOccurred = true;
    return;
  }

  parser = XML_ParserCreate(nullptr);
  if (!parser) {
    allocationFailed = true;
    errorOccurred = true;
  } else {
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, characterData);
  }
}

WebDAVPropfindParser::~WebDAVPropfindParser() { destroyXmlParser(parser); }

bool WebDAVPropfindParser::feed(const char* xmlData, size_t length) {
  if (errorOccurred) return false;

  const char* currentPos = xmlData;
  size_t remaining = length;
  constexpr size_t chunkSize = 1024;

  while (remaining > 0) {
    void* const buf = XML_GetBuffer(parser, chunkSize);
    if (!buf) {
      allocationFailed = XML_GetErrorCode(parser) == XML_ERROR_NO_MEMORY;
      errorOccurred = true;
      destroyXmlParser(parser);
      return false;
    }

    const size_t toRead = remaining < chunkSize ? remaining : chunkSize;
    memcpy(buf, currentPos, toRead);

    if (XML_ParseBuffer(parser, static_cast<int>(toRead), XML_FALSE) == XML_STATUS_ERROR) {
      allocationFailed = XML_GetErrorCode(parser) == XML_ERROR_NO_MEMORY;
      errorOccurred = true;
      destroyXmlParser(parser);
      return false;
    }
    currentPos += toRead;
    remaining -= toRead;
  }
  return true;
}

bool WebDAVPropfindParser::finish() {
  if (errorOccurred) return false;
  if (XML_Parse(parser, nullptr, 0, XML_TRUE) != XML_STATUS_OK) {
    allocationFailed = XML_GetErrorCode(parser) == XML_ERROR_NO_MEMORY;
    errorOccurred = true;
    destroyXmlParser(parser);
    return false;
  }
  return true;
}

void XMLCALL WebDAVPropfindParser::startElement(void* userData, const XML_Char* name,
                                                const XML_Char** /*atts*/) {
  auto* self = static_cast<WebDAVPropfindParser*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    self->inResponse = true;
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

void XMLCALL WebDAVPropfindParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<WebDAVPropfindParser*>(userData);
  const char* local = localName(name);
  if (strcmp(local, "response") == 0) {
    if (!self->current.href.empty()) {
      self->current.isDirectory = self->currentIsCollection;
      self->current.name = hrefToName(self->current.href);
      if (self->current.name.empty()) {
        self->current.name = self->current.href;
      }
      if (!self->items.push_back(std::move(self->current))) {
        self->truncated = true;
      }
    }
    self->inResponse = false;
  } else if (strcmp(local, "href") == 0) {
    self->inHref = false;
    if (self->inResponse) {
      if (self->items.full()) {
        // Keep parsing the XML stream, but avoid allocating href/name strings
        // for entries that cannot fit in the bounded listing.
        self->truncated = true;
      } else {
        self->current.href = self->currentText;
      }
    }
  } else if (strcmp(local, "resourcetype") == 0) {
    self->inResourceType = false;
  } else if (strcmp(local, "getcontentlength") == 0) {
    self->inContentLength = false;
    if (self->inResponse) {
      self->current.size = strtoull(self->currentText.c_str(), nullptr, 10);
    }
  }
}

void XMLCALL WebDAVPropfindParser::characterData(void* userData, const XML_Char* s, int len) {
  auto* self = static_cast<WebDAVPropfindParser*>(userData);
  if (self->inHref || self->inContentLength) {
    self->currentText.append(s, len);
  }
}
