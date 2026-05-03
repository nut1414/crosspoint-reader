#pragma once

#include <string>
#include <vector>

#include "network/HttpDownloader.h"

struct WebDAVItem {
  std::string href;      // URL-encoded remote path
  std::string name;      // decoded filename
  bool isDirectory;
  size_t size;
};

enum class WebDAVError {
  OK,
  HTTP_ERROR,
  AUTH_ERROR,
  PARSE_ERROR,
  NETWORK_ERROR
};

class WebDAVClient {
 public:
  /**
   * Perform a PROPFIND request to list directory contents.
   * @param url The directory URL to probe
   * @param out Vector to populate with child items
   * @param username Basic auth username
   * @param password Basic auth password
   * @return WebDAVError::OK on success
   */
  static WebDAVError propfind(const std::string& url, std::vector<WebDAVItem>& out,
                              const std::string& username = "", const std::string& password = "");

  /**
   * Download a file from the WebDAV server.
   * Delegates to HttpDownloader::downloadToFile.
   * @param url Full file URL
   * @param destPath Local SD card destination path
   * @param progress Optional progress callback
   * @param username Basic auth username
   * @param password Basic auth password
   * @return WebDAVError::OK on success
   */
  static WebDAVError downloadFile(const std::string& url, const std::string& destPath,
                                  HttpDownloader::ProgressCallback progress = nullptr,
                                  const std::string& username = "", const std::string& password = "");
};
