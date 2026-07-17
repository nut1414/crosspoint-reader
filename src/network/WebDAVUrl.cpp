#include "network/WebDAVUrl.h"

#include <utility>

#include "util/UrlUtils.h"

namespace {

std::string stripQueryAndFragment(std::string url) {
  const size_t queryPos = url.find_first_of("?#");
  if (queryPos != std::string::npos) {
    url.resize(queryPos);
  }
  return url;
}

std::string pathFromUrl(const std::string& url) {
  const size_t protocolEnd = url.find("://");
  const size_t pathStart = protocolEnd == std::string::npos ? url.find('/') : url.find('/', protocolEnd + 3);
  if (pathStart == std::string::npos) {
    return "/";
  }

  std::string path = url.substr(pathStart);
  const size_t queryPos = path.find_first_of("?#");
  if (queryPos != std::string::npos) {
    path.resize(queryPos);
  }
  return path.empty() ? "/" : path;
}

std::string ensureTrailingSlash(std::string url) {
  url = stripQueryAndFragment(std::move(url));
  if (url.empty() || url.back() == '/') {
    return url;
  }
  return url + "/";
}

std::string withoutTrailingSlash(std::string value) {
  while (value.size() > 1 && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

bool startsWithPath(const std::string& path, const std::string& basePath) {
  const std::string normalizedBase = ensureTrailingSlash(basePath.empty() ? "/" : basePath);
  if (path == withoutTrailingSlash(normalizedBase)) {
    return true;
  }
  return path.rfind(normalizedBase, 0) == 0;
}

std::string appendRelativePath(const std::string& collectionUrl, std::string path) {
  while (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  while (path.rfind("./", 0) == 0) {
    path.erase(0, 2);
  }

  std::string base = ensureTrailingSlash(UrlUtils::ensureProtocol(collectionUrl));
  return base + path;
}

}  // namespace

namespace WebDAVUrl {

std::string normalizeCollectionUrl(const std::string& url) {
  if (url.empty()) {
    return "";
  }
  return ensureTrailingSlash(UrlUtils::ensureProtocol(url));
}

std::string resolveHref(const std::string& baseUrl, const std::string& currentCollectionUrl,
                        const std::string& href) {
  if (href.empty()) {
    return normalizeCollectionUrl(currentCollectionUrl.empty() ? baseUrl : currentCollectionUrl);
  }

  if (href.find("://") != std::string::npos) {
    return href;
  }

  const std::string effectiveBase = normalizeCollectionUrl(baseUrl.empty() ? currentCollectionUrl : baseUrl);
  const std::string effectiveCurrent =
      normalizeCollectionUrl(currentCollectionUrl.empty() ? effectiveBase : currentCollectionUrl);
  if (effectiveBase.empty()) {
    return href;
  }

  if (href.front() == '/') {
    const std::string host = UrlUtils::extractHost(effectiveBase);
    const std::string basePath = pathFromUrl(effectiveBase);
    const std::string hrefPath = pathFromUrl(href);

    if (startsWithPath(hrefPath, basePath)) {
      return host + href;
    }
    return appendRelativePath(effectiveBase, href);
  }

  return appendRelativePath(effectiveCurrent, href);
}

bool sameResource(const std::string& lhs, const std::string& rhs) {
  return withoutTrailingSlash(stripQueryAndFragment(UrlUtils::ensureProtocol(lhs))) ==
         withoutTrailingSlash(stripQueryAndFragment(UrlUtils::ensureProtocol(rhs)));
}

std::vector<WebDAVItem> resolveItems(const std::string& baseUrl, const std::string& currentCollectionUrl,
                                     std::vector<WebDAVItem>&& items) {
  size_t writeIndex = 0;
  for (size_t readIndex = 0; readIndex < items.size(); ++readIndex) {
    auto& item = items[readIndex];
    item.href = resolveHref(baseUrl, currentCollectionUrl, item.href);
    if (sameResource(item.href, currentCollectionUrl)) {
      continue;
    }
    if (writeIndex != readIndex) {
      items[writeIndex] = std::move(item);
    }
    ++writeIndex;
  }
  items.resize(writeIndex);

  return std::move(items);
}

}  // namespace WebDAVUrl
