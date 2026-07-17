#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "src/network/WebDAVPropfindParser.h"
#include "src/network/WebDAVUrl.h"

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_EQ(a, b)                                                           \
  do {                                                                            \
    auto _a = (a);                                                                \
    auto _b = (b);                                                                \
    if (_a != _b) {                                                               \
      fprintf(stderr, "  FAIL: %s:%d: %s != expected\n", __FILE__, __LINE__, #a); \
      fprintf(stderr, "        actual:   %s\n", std::string(_a).c_str());         \
      fprintf(stderr, "        expected: %s\n", std::string(_b).c_str());         \
      testsFailed++;                                                              \
      return;                                                                     \
    }                                                                             \
  } while (0)

#define ASSERT_SIZE(a, b)                                                           \
  do {                                                                              \
    auto _a = (a);                                                                  \
    auto _b = (b);                                                                  \
    if (_a != _b) {                                                                 \
      fprintf(stderr, "  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);     \
      testsFailed++;                                                               \
      return;                                                                      \
    }                                                                               \
  } while (0)

#define ASSERT_TRUE(cond)                                                \
  do {                                                                   \
    if (!(cond)) {                                                       \
      fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      testsFailed++;                                                     \
      return;                                                            \
    }                                                                    \
  } while (0)

#define PASS() testsPassed++

static std::vector<WebDAVItem> parsePropfind(const std::string& xml) {
  WebDAVPropfindParser parser;
  parser.feed(xml.data(), xml.size());
  parser.finish();
  if (parser.error()) {
    fprintf(stderr, "  FAIL: %s:%d: parser.error()\n", __FILE__, __LINE__);
    testsFailed++;
    return {};
  }
  return std::move(parser.items);
}

void testResolveRootRelativeHrefUnderConfiguredBase() {
  printf("testResolveRootRelativeHrefUnderConfiguredBase...\n");

  ASSERT_EQ(WebDAVUrl::resolveHref("https://host/webdav/", "https://host/webdav/", "/Folder/"),
            "https://host/webdav/Folder/");

  printf("  passed\n");
  PASS();
}

void testResolveAbsolutePathHrefThatAlreadyIncludesBasePath() {
  printf("testResolveAbsolutePathHrefThatAlreadyIncludesBasePath...\n");

  ASSERT_EQ(WebDAVUrl::resolveHref("https://host/webdav/", "https://host/webdav/", "/webdav/Folder/"),
            "https://host/webdav/Folder/");

  printf("  passed\n");
  PASS();
}

void testResolveRelativeHrefAgainstCurrentCollection() {
  printf("testResolveRelativeHrefAgainstCurrentCollection...\n");

  ASSERT_EQ(WebDAVUrl::resolveHref("https://host/webdav/", "https://host/webdav/", "Folder/"),
            "https://host/webdav/Folder/");
  ASSERT_EQ(WebDAVUrl::resolveHref("https://host/webdav/", "https://host/webdav/Parent/", "Child/"),
            "https://host/webdav/Parent/Child/");

  printf("  passed\n");
  PASS();
}

void testResolveAbsoluteUrlAsIs() {
  printf("testResolveAbsoluteUrlAsIs...\n");

  ASSERT_EQ(WebDAVUrl::resolveHref("https://host/webdav/", "https://host/webdav/",
                                   "https://other.example/dav/Folder/"),
            "https://other.example/dav/Folder/");

  printf("  passed\n");
  PASS();
}

void testResolveItemsSkipsCurrentCollectionByUrl() {
  printf("testResolveItemsSkipsCurrentCollectionByUrl...\n");

  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<D:multistatus xmlns:D=\"DAV:\">"
      "<D:response>"
      "<D:href>/webdav/Book.epub</D:href>"
      "<D:propstat><D:prop><D:resourcetype/>"
      "<D:getcontentlength>123</D:getcontentlength>"
      "</D:prop></D:propstat>"
      "</D:response>"
      "<D:response>"
      "<D:href>/webdav/</D:href>"
      "<D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat>"
      "</D:response>"
      "<D:response>"
      "<D:href>/Folder/</D:href>"
      "<D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat>"
      "</D:response>"
      "</D:multistatus>";

  auto rawItems = parsePropfind(xml);
  ASSERT_SIZE(rawItems.size(), 3u);

  auto items = WebDAVUrl::resolveItems("https://host/webdav/", "https://host/webdav/", std::move(rawItems));
  ASSERT_SIZE(items.size(), 2u);
  ASSERT_EQ(items[0].href, "https://host/webdav/Book.epub");
  ASSERT_EQ(items[0].name, "Book.epub");
  ASSERT_TRUE(!items[0].isDirectory);
  ASSERT_SIZE(items[0].size, 123u);
  ASSERT_EQ(items[1].href, "https://host/webdav/Folder/");
  ASSERT_EQ(items[1].name, "Folder");
  ASSERT_TRUE(items[1].isDirectory);

  printf("  passed\n");
  PASS();
}

void testResolveItemsReusesIncomingStorageWhileFilteringInOrder() {
  printf("testResolveItemsReusesIncomingStorageWhileFilteringInOrder...\n");

  std::vector<WebDAVItem> rawItems{
      {"/webdav/First.epub", "First.epub", false, 11},
      {"/webdav/", "webdav", true, 0},
      {"/webdav/Second.epub", "Second.epub", false, 22},
  };
  rawItems.reserve(200);
  const WebDAVItem* const incomingStorage = rawItems.data();
  const size_t incomingCapacity = rawItems.capacity();

  auto items = WebDAVUrl::resolveItems("https://host/webdav/", "https://host/webdav/", std::move(rawItems));

  ASSERT_TRUE(items.data() == incomingStorage);
  ASSERT_SIZE(items.capacity(), incomingCapacity);
  ASSERT_SIZE(items.size(), 2u);
  ASSERT_EQ(items[0].href, "https://host/webdav/First.epub");
  ASSERT_EQ(items[0].name, "First.epub");
  ASSERT_SIZE(items[0].size, 11u);
  ASSERT_EQ(items[1].href, "https://host/webdav/Second.epub");
  ASSERT_EQ(items[1].name, "Second.epub");
  ASSERT_SIZE(items[1].size, 22u);

  printf("  passed\n");
  PASS();
}

void testParserReusesPreviousListingStorage() {
  printf("testParserReusesPreviousListingStorage...\n");

  std::vector<WebDAVItem> previousItems;
  previousItems.reserve(200);
  previousItems.push_back({"stale", "stale", false, 0});
  const WebDAVItem* const incomingStorage = previousItems.data();

  WebDAVPropfindParser parser{std::move(previousItems)};
  ASSERT_TRUE(parser.items.empty());
  parser.items.push_back({"fresh", "fresh", false, 0});
  ASSERT_TRUE(parser.items.data() == incomingStorage);

  printf("  passed\n");
  PASS();
}

int main() {
  testResolveRootRelativeHrefUnderConfiguredBase();
  testResolveAbsolutePathHrefThatAlreadyIncludesBasePath();
  testResolveRelativeHrefAgainstCurrentCollection();
  testResolveAbsoluteUrlAsIs();
  testResolveItemsSkipsCurrentCollectionByUrl();
  testResolveItemsReusesIncomingStorageWhileFilteringInOrder();
  testParserReusesPreviousListingStorage();

  printf("\nTests passed: %d\n", testsPassed);
  if (testsFailed > 0) {
    printf("Tests failed: %d\n", testsFailed);
    return 1;
  }
  return 0;
}
