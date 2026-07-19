#pragma once

#include <string>

#include "network/HttpDownloader.h"
#include "network/WebDAVItemList.h"
#include "network/WebDAVTypes.h"

class WebDAVClient {
 public:
  /**
   * Perform a PROPFIND request to list directory contents.
   * @param url The directory URL to probe
   * @param out Reusable list to populate with child items
   * @param username Basic auth username
   * @param password Basic auth password
   * @param truncated Optional out-param set to true if the server returned more
   *        items than the internal limit and the result was truncated
   * @param baseUrl Configured WebDAV collection root used to normalize returned hrefs
   * @return WebDAVError::OK on success
   */
  static WebDAVError propfind(const std::string& url, WebDAVItemList& out,
                              const std::string& username = "", const std::string& password = "",
                              bool* truncated = nullptr, const std::string& baseUrl = "");

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
