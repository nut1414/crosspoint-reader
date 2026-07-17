#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <expat.h>

#include "network/WebDAVTypes.h"

class WebDAVPropfindParser {
 public:
  explicit WebDAVPropfindParser(std::vector<WebDAVItem>&& reusableItems = {});
  ~WebDAVPropfindParser();

  bool feed(const char* xmlData, size_t length);
  bool finish();
  bool error() const { return errorOccurred; }

  std::vector<WebDAVItem> items;
  bool truncated = false;

 private:
  static constexpr size_t MAX_ITEMS = 200;
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);

  XML_Parser parser = nullptr;
  WebDAVItem current;
  std::string currentText;
  bool inResponse = false;
  bool inHref = false;
  bool inResourceType = false;
  bool inContentLength = false;
  bool currentIsCollection = false;
  bool errorOccurred = false;
};
