#pragma once

#include <cstddef>
#include <string>

struct WebDAVItem {
  std::string href;      // URL-encoded remote path, normalized to an absolute URL after PROPFIND
  std::string name;      // decoded filename
  bool isDirectory = false;
  size_t size = 0;
};

enum class WebDAVError {
  OK,
  HTTP_ERROR,
  AUTH_ERROR,
  PARSE_ERROR,
  NETWORK_ERROR,
  OUT_OF_MEMORY
};
