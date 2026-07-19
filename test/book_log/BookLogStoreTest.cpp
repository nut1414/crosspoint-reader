#include <gtest/gtest.h>

#include <HalStorage.h>

#include <string>
#include <vector>

#include "BookLogStore.h"
#include "RecentBooksStore.h"

HalStorage HalStorage::instance;

namespace {
constexpr char BOOK_LOG_PATH[] = "/.crosspoint/book_log.json";

class BookLogStoreTest : public testing::Test {
 protected:
  void SetUp() override { Storage.reset(); }
};

TEST_F(BookLogStoreTest, RecordsUniqueBooksNewestFirstAndRefreshesMetadata) {
  BookLogStore store;
  store.recordBook("/one.epub", "One", "Author One");
  store.recordBook("/two.xtc", "Two", "Author Two");
  store.recordBook("/one.epub", "One Revised", "New Author");

  ASSERT_EQ(store.getCount(), 2);
  EXPECT_EQ(store.getBooks()[0].path, "/one.epub");
  EXPECT_EQ(store.getBooks()[0].title, "One Revised");
  EXPECT_EQ(store.getBooks()[0].author, "New Author");
  EXPECT_EQ(store.getBooks()[1].path, "/two.xtc");
}

TEST_F(BookLogStoreTest, HasNoRecentBooksStyleLimit) {
  BookLogStore store;
  for (int i = 0; i < 40; i++) {
    store.recordBook("/book-" + std::to_string(i) + ".epub", "Book " + std::to_string(i), "");
  }

  ASSERT_EQ(store.getCount(), 40);
  EXPECT_EQ(store.getBooks().front().path, "/book-39.epub");
  EXPECT_EQ(store.getBooks().back().path, "/book-0.epub");
}

TEST_F(BookLogStoreTest, RemovesAndRepointsEntries) {
  BookLogStore store;
  store.recordBook("/old.epub", "Old", "Author");
  store.recordBook("/other.epub", "Other", "");

  store.updatePath("/old.epub", "/Read/old.epub");
  ASSERT_EQ(store.getCount(), 2);
  EXPECT_EQ(store.getBooks()[1].path, "/Read/old.epub");
  EXPECT_EQ(store.getBooks()[1].title, "Old");

  EXPECT_TRUE(store.removeByPath("/Read/old.epub"));
  EXPECT_FALSE(store.removeByPath("/Read/old.epub"));
  ASSERT_EQ(store.getCount(), 1);
  EXPECT_EQ(store.getBooks()[0].path, "/other.epub");
}

TEST_F(BookLogStoreTest, PathUpdateDeduplicatesAnExistingDestination) {
  BookLogStore store;
  store.recordBook("/destination.epub", "Earlier Destination", "");
  store.recordBook("/source.epub", "Current Source", "Author");

  store.updatePath("/source.epub", "/destination.epub");

  ASSERT_EQ(store.getCount(), 1);
  EXPECT_EQ(store.getBooks()[0].path, "/destination.epub");
  EXPECT_EQ(store.getBooks()[0].title, "Current Source");
  EXPECT_EQ(store.getBooks()[0].author, "Author");
}

TEST_F(BookLogStoreTest, MissingStateDoesNotRemoveHistory) {
  BookLogStore store;
  store.recordBook("/kept.epub", "Kept", "");

  Storage.markPathExisting("/kept.epub");
  EXPECT_FALSE(BookLogStore::isMissing(store.getBooks()[0]));

  Storage.markPathMissing("/kept.epub");
  EXPECT_TRUE(BookLogStore::isMissing(store.getBooks()[0]));
  EXPECT_EQ(store.getCount(), 1);
}

TEST_F(BookLogStoreTest, MigratesRecentsOnceAndPersistsEmptyMigrationMarker) {
  const std::vector<RecentBook> recents = {
      {"/new.epub", "New", "Author", "/cover.bmp"},
      {"/old.txt", "Old", "", ""},
      {"/new.epub", "Duplicate", "", ""},
  };

  BookLogStore migrated;
  EXPECT_TRUE(migrated.loadOrInitializeFromRecentBooks(recents));
  ASSERT_EQ(migrated.getCount(), 2);
  EXPECT_EQ(migrated.getBooks()[0].title, "New");
  EXPECT_TRUE(Storage.exists(BOOK_LOG_PATH));

  BookLogStore reloaded;
  EXPECT_TRUE(reloaded.loadOrInitializeFromRecentBooks({}));
  ASSERT_EQ(reloaded.getCount(), 2);
  EXPECT_EQ(reloaded.getBooks()[1].path, "/old.txt");

  Storage.reset();
  BookLogStore empty;
  EXPECT_TRUE(empty.loadOrInitializeFromRecentBooks({}));
  EXPECT_EQ(empty.getCount(), 0);
  EXPECT_TRUE(Storage.exists(BOOK_LOG_PATH));
}

TEST_F(BookLogStoreTest, RoundTripsVersionedJson) {
  BookLogStore saved;
  saved.recordBook("/book.md", "Book", "Writer");
  saved.recordBook("/second.epub", "Second", "");

  EXPECT_NE(Storage.getFile(BOOK_LOG_PATH).find("\"version\":1"), std::string::npos);

  BookLogStore loaded;
  bool fileFound = false;
  EXPECT_TRUE(loaded.loadFromFile(&fileFound));
  EXPECT_TRUE(fileFound);
  ASSERT_EQ(loaded.getCount(), 2);
  EXPECT_EQ(loaded.getBooks()[0].path, "/second.epub");
  EXPECT_EQ(loaded.getBooks()[1].author, "Writer");
}

TEST_F(BookLogStoreTest, IgnoresMalformedAndDuplicateRecords) {
  Storage.setFile(BOOK_LOG_PATH,
                  R"({"version":1,"books":[{"path":"/valid.epub","title":"Valid","author":"A"},{"title":"No path"},{"path":""},{"path":"/valid.epub","title":"Duplicate"},{"path":"/fallback.txt"}]})");

  BookLogStore store;
  EXPECT_TRUE(store.loadFromFile());
  ASSERT_EQ(store.getCount(), 2);
  EXPECT_EQ(store.getBooks()[0].title, "Valid");
  EXPECT_EQ(store.getBooks()[1].title, "fallback.txt");
}

TEST_F(BookLogStoreTest, RejectsCorruptOrUnsupportedFilesWithoutReplacingCurrentData) {
  BookLogStore store;
  store.recordBook("/kept.epub", "Kept", "");

  Storage.setFile(BOOK_LOG_PATH, "not-json");
  EXPECT_FALSE(store.loadFromFile());
  ASSERT_EQ(store.getCount(), 1);
  EXPECT_EQ(store.getBooks()[0].path, "/kept.epub");

  Storage.setFile(BOOK_LOG_PATH, R"({"version":99,"books":[]})");
  EXPECT_FALSE(store.loadFromFile());
  ASSERT_EQ(store.getCount(), 1);
}
}  // namespace
