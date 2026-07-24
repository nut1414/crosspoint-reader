#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "src/network/HttpChunkDecoder.h"
#include "src/network/WebDAVItemList.h"
#include "src/network/WebDAVPropfindParser.h"
#include "src/network/WebDAVResponseEndDetector.h"
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

static bool parsePropfind(const std::string& xml, WebDAVItemList& items, bool* truncated = nullptr) {
  WebDAVPropfindParser parser{items};
  if (parser.outOfMemory()) {
    fprintf(stderr, "  FAIL: %s:%d: parser.outOfMemory()\n", __FILE__, __LINE__);
    testsFailed++;
    return false;
  }
  parser.feed(xml.data(), xml.size());
  parser.finish();
  if (parser.error()) {
    fprintf(stderr, "  FAIL: %s:%d: parser.error()\n", __FILE__, __LINE__);
    testsFailed++;
    return false;
  }
  if (truncated) *truncated = parser.truncated;
  return true;
}

static WebDAVItemList::StoragePtr failStorageAllocation(size_t) noexcept { return {}; }

static WebDAVItemList::StoragePtr allocateAtMost64Items(const size_t count) noexcept {
  if (count > 64) return {};
  return WebDAVItemList::StoragePtr{new (std::nothrow) WebDAVItem[count]};
}

static bool decodeChunked(const std::string& encoded, std::string& decoded) {
  HttpChunkDecoder decoder;
  for (const unsigned char byte : encoded) {
    const auto result = decoder.consume(byte);
    if (result == HttpChunkDecoder::Result::DATA) decoded.push_back(static_cast<char>(byte));
    if (result == HttpChunkDecoder::Result::ERROR) return false;
    if (result == HttpChunkDecoder::Result::COMPLETE) return true;
  }
  return decoder.complete();
}

void testChunkDecoderHandlesExtensionsAndBoundaries() {
  printf("testChunkDecoderHandlesExtensionsAndBoundaries...\n");

  std::string decoded;
  ASSERT_TRUE(decodeChunked("4;name=value\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", decoded));
  ASSERT_EQ(decoded, "Wikipedia");

  printf("  passed\n");
  PASS();
}

void testChunkDecoderRejectsMalformedOrIncompleteFraming() {
  printf("testChunkDecoderRejectsMalformedOrIncompleteFraming...\n");

  std::string decoded;
  ASSERT_TRUE(!decodeChunked("1\r\nA\n0\r\n\r\n", decoded));
  decoded.clear();
  ASSERT_TRUE(!decodeChunked("4\r\nWiki\r\n", decoded));

  printf("  passed\n");
  PASS();
}

void testResponseEndDetectorFindsPrefixedAndPlainRootClose() {
  printf("testResponseEndDetectorFindsPrefixedAndPlainRootClose...\n");

  WebDAVResponseEndDetector prefixed;
  const std::string prefixedXml = "<D:multistatus><D:response/></D:multistatus>";
  for (const unsigned char byte : prefixedXml) prefixed.consume(byte);
  ASSERT_TRUE(prefixed.complete());

  WebDAVResponseEndDetector plain;
  const std::string plainXml = "<multistatus><response/></multistatus   >";
  for (const unsigned char byte : plainXml) plain.consume(byte);
  ASSERT_TRUE(plain.complete());

  printf("  passed\n");
  PASS();
}

void testResponseEndDetectorIgnoresOpeningAndChildTags() {
  printf("testResponseEndDetectorIgnoresOpeningAndChildTags...\n");

  WebDAVResponseEndDetector detector;
  const std::string incompleteXml = "<D:multistatus><D:response></D:response>";
  for (const unsigned char byte : incompleteXml) detector.consume(byte);
  ASSERT_TRUE(!detector.complete());

  printf("  passed\n");
  PASS();
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

  WebDAVItemList rawItems;
  ASSERT_TRUE(parsePropfind(xml, rawItems));
  ASSERT_SIZE(rawItems.size(), 3u);

  WebDAVUrl::resolveItems("https://host/webdav/", "https://host/webdav/", rawItems);
  ASSERT_SIZE(rawItems.size(), 2u);
  ASSERT_EQ(rawItems[0].href, "https://host/webdav/Book.epub");
  ASSERT_EQ(rawItems[0].name, "Book.epub");
  ASSERT_TRUE(!rawItems[0].isDirectory);
  ASSERT_SIZE(rawItems[0].size, 123u);
  ASSERT_EQ(rawItems[1].href, "https://host/webdav/Folder/");
  ASSERT_EQ(rawItems[1].name, "Folder");
  ASSERT_TRUE(rawItems[1].isDirectory);

  printf("  passed\n");
  PASS();
}

void testResolveItemsReusesIncomingStorageWhileFilteringInOrder() {
  printf("testResolveItemsReusesIncomingStorageWhileFilteringInOrder...\n");

  WebDAVItemList rawItems;
  ASSERT_TRUE(rawItems.ensureStorage());
  ASSERT_TRUE(rawItems.push_back({"/webdav/First.epub", "First.epub", false, 11}));
  ASSERT_TRUE(rawItems.push_back({"/webdav/", "webdav", true, 0}));
  ASSERT_TRUE(rawItems.push_back({"/webdav/Second.epub", "Second.epub", false, 22}));
  const WebDAVItem* const incomingStorage = rawItems.data();
  const size_t incomingCapacity = rawItems.capacity();

  WebDAVUrl::resolveItems("https://host/webdav/", "https://host/webdav/", rawItems);

  ASSERT_TRUE(rawItems.data() == incomingStorage);
  ASSERT_SIZE(rawItems.capacity(), incomingCapacity);
  ASSERT_SIZE(rawItems.size(), 2u);
  ASSERT_EQ(rawItems[0].href, "https://host/webdav/First.epub");
  ASSERT_EQ(rawItems[0].name, "First.epub");
  ASSERT_SIZE(rawItems[0].size, 11u);
  ASSERT_EQ(rawItems[1].href, "https://host/webdav/Second.epub");
  ASSERT_EQ(rawItems[1].name, "Second.epub");
  ASSERT_SIZE(rawItems[1].size, 22u);

  printf("  passed\n");
  PASS();
}

void testParserReusesPreviousListingStorage() {
  printf("testParserReusesPreviousListingStorage...\n");

  WebDAVItemList previousItems;
  ASSERT_TRUE(previousItems.ensureStorage());
  ASSERT_TRUE(previousItems.push_back({"stale", "stale", false, 0}));
  const WebDAVItem* const incomingStorage = previousItems.data();

  WebDAVPropfindParser parser{previousItems};
  ASSERT_TRUE(previousItems.empty());
  ASSERT_TRUE(previousItems.push_back({"fresh", "fresh", false, 0}));
  ASSERT_TRUE(previousItems.data() == incomingStorage);

  printf("  passed\n");
  PASS();
}

void testParserReportsStorageAllocationFailure() {
  printf("testParserReportsStorageAllocationFailure...\n");

  WebDAVItemList items{&failStorageAllocation};
  WebDAVPropfindParser parser{items};

  ASSERT_TRUE(parser.outOfMemory());
  ASSERT_TRUE(parser.error());
  ASSERT_TRUE(items.empty());
  ASSERT_SIZE(items.capacity(), 0u);

  printf("  passed\n");
  PASS();
}

void testListingFallsBackToSmallerContiguousStorage() {
  printf("testListingFallsBackToSmallerContiguousStorage...\n");

  WebDAVItemList items{&allocateAtMost64Items};
  WebDAVPropfindParser parser{items};

  ASSERT_TRUE(!parser.error());
  ASSERT_SIZE(items.capacity(), 64u);

  printf("  passed\n");
  PASS();
}

void testParserHonorsRequestedListingCapacity() {
  printf("testParserHonorsRequestedListingCapacity...\n");

  WebDAVItemList items;
  WebDAVPropfindParser parser{items, 64};
  ASSERT_TRUE(!parser.error());
  ASSERT_SIZE(items.capacity(), 64u);

  std::string xml = "<D:multistatus xmlns:D=\"DAV:\">";
  for (size_t i = 0; i <= 64; ++i) {
    xml += "<D:response><D:href>/webdav/Book" + std::to_string(i) +
           ".epub</D:href></D:response>";
  }
  xml += "</D:multistatus>";

  ASSERT_TRUE(parser.feed(xml.data(), xml.size()));
  ASSERT_TRUE(parser.finish());
  ASSERT_SIZE(items.size(), 64u);
  ASSERT_TRUE(parser.truncated);

  printf("  passed\n");
  PASS();
}

void testListingCanReleaseRetainedStorageBeforeNetworkRequest() {
  printf("testListingCanReleaseRetainedStorageBeforeNetworkRequest...\n");

  WebDAVItemList items;
  ASSERT_TRUE(items.ensureStorage(64));
  ASSERT_TRUE(items.push_back({"/webdav/Book.epub", "Book.epub", false, 123}));
  ASSERT_TRUE(items.data() != nullptr);
  ASSERT_SIZE(items.capacity(), 64u);

  items.releaseStorage();

  ASSERT_TRUE(items.empty());
  ASSERT_TRUE(items.data() == nullptr);
  ASSERT_SIZE(items.capacity(), 0u);
  ASSERT_TRUE(items.ensureStorage(32));
  ASSERT_SIZE(items.capacity(), 32u);

  printf("  passed\n");
  PASS();
}

void testMalformedResponseRetryRetainsStorage() {
  printf("testMalformedResponseRetryRetainsStorage...\n");

  WebDAVItemList items;
  ASSERT_TRUE(items.ensureStorage());
  const WebDAVItem* const storage = items.data();

  {
    WebDAVPropfindParser parser{items};
    const std::string malformed = "<D:multistatus xmlns:D=\"DAV:\"><D:response>";
    ASSERT_TRUE(parser.feed(malformed.data(), malformed.size()));
    ASSERT_TRUE(!parser.finish());
    ASSERT_TRUE(parser.error());
    ASSERT_TRUE(parser.xmlError() != XML_ERROR_NONE);
  }

  items.clear();
  ASSERT_TRUE(items.data() == storage);

  const std::string valid =
      "<D:multistatus xmlns:D=\"DAV:\">"
      "<D:response><D:href>/webdav/Book.epub</D:href></D:response>"
      "</D:multistatus>";
  ASSERT_TRUE(parsePropfind(valid, items));
  ASSERT_TRUE(items.data() == storage);
  ASSERT_SIZE(items.size(), 1u);

  printf("  passed\n");
  PASS();
}

void testParserTruncatesWithoutGrowingStorage() {
  printf("testParserTruncatesWithoutGrowingStorage...\n");

  std::string xml = "<D:multistatus xmlns:D=\"DAV:\">";
  for (size_t i = 0; i <= WebDAVItemList::MAX_ITEMS; ++i) {
    xml += "<D:response><D:href>/webdav/Book" + std::to_string(i) +
           ".epub</D:href></D:response>";
  }
  xml += "</D:multistatus>";

  WebDAVItemList items;
  bool truncated = false;
  ASSERT_TRUE(parsePropfind(xml, items, &truncated));
  ASSERT_SIZE(items.capacity(), WebDAVItemList::MAX_ITEMS);
  ASSERT_SIZE(items.size(), WebDAVItemList::MAX_ITEMS);
  ASSERT_TRUE(truncated);

  printf("  passed\n");
  PASS();
}

int main() {
  testChunkDecoderHandlesExtensionsAndBoundaries();
  testChunkDecoderRejectsMalformedOrIncompleteFraming();
  testResponseEndDetectorFindsPrefixedAndPlainRootClose();
  testResponseEndDetectorIgnoresOpeningAndChildTags();
  testResolveRootRelativeHrefUnderConfiguredBase();
  testResolveAbsolutePathHrefThatAlreadyIncludesBasePath();
  testResolveRelativeHrefAgainstCurrentCollection();
  testResolveAbsoluteUrlAsIs();
  testResolveItemsSkipsCurrentCollectionByUrl();
  testResolveItemsReusesIncomingStorageWhileFilteringInOrder();
  testParserReusesPreviousListingStorage();
  testParserReportsStorageAllocationFailure();
  testListingFallsBackToSmallerContiguousStorage();
  testParserHonorsRequestedListingCapacity();
  testListingCanReleaseRetainedStorageBeforeNetworkRequest();
  testMalformedResponseRetryRetainsStorage();
  testParserTruncatesWithoutGrowingStorage();

  printf("\nTests passed: %d\n", testsPassed);
  if (testsFailed > 0) {
    printf("Tests failed: %d\n", testsFailed);
    return 1;
  }
  return 0;
}
