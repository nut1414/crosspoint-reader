#pragma once

#include <cstddef>
#include <string>

#include <expat.h>

#include "network/WebDAVItemList.h"
#include "network/WebDAVTypes.h"

class WebDAVPropfindParser {
 public:
  explicit WebDAVPropfindParser(WebDAVItemList& output);
  ~WebDAVPropfindParser();

  WebDAVPropfindParser(const WebDAVPropfindParser&) = delete;
  WebDAVPropfindParser& operator=(const WebDAVPropfindParser&) = delete;
  WebDAVPropfindParser(WebDAVPropfindParser&&) = delete;
  WebDAVPropfindParser& operator=(WebDAVPropfindParser&&) = delete;

  bool feed(const char* xmlData, size_t length);
  bool finish();
  bool error() const { return errorOccurred; }
  bool outOfMemory() const { return allocationFailed; }

  bool truncated = false;

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);

  XML_Parser parser = nullptr;
  WebDAVItemList& items;
  WebDAVItem current;
  std::string currentText;
  bool inResponse = false;
  bool inHref = false;
  bool inResourceType = false;
  bool inContentLength = false;
  bool currentIsCollection = false;
  bool errorOccurred = false;
  bool allocationFailed = false;
};
