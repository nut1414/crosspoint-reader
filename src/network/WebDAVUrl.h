#pragma once

#include <string>
#include <vector>

#include "network/WebDAVTypes.h"

namespace WebDAVUrl {

std::string normalizeCollectionUrl(const std::string& url);
std::string resolveHref(const std::string& baseUrl, const std::string& currentCollectionUrl, const std::string& href);
bool sameResource(const std::string& lhs, const std::string& rhs);
std::vector<WebDAVItem> resolveItems(const std::string& baseUrl, const std::string& currentCollectionUrl,
                                     std::vector<WebDAVItem>&& items);

}  // namespace WebDAVUrl
