#include "BookLogStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdint>
#include <unordered_set>

#include "RecentBooksStore.h"

namespace {
constexpr uint8_t BOOK_LOG_FILE_VERSION = 1;
constexpr char BOOK_LOG_FILE_JSON[] = "/.crosspoint/book_log.json";

std::string titleOrFilename(const std::string& title, const std::string& path) {
  if (!title.empty()) return title;
  const size_t lastSlash = path.find_last_of('/');
  return lastSlash == std::string::npos ? path : path.substr(lastSlash + 1);
}
}  // namespace

BookLogStore BookLogStore::instance;

void BookLogStore::recordBook(const std::string& path, const std::string& title, const std::string& author) {
  if (path.empty()) return;

  auto it = std::find_if(books.begin(), books.end(),
                         [&](const BookLogEntry& book) { return book.path == path; });
  if (it != books.end()) {
    books.erase(it);
  }

  books.insert(books.begin(), {path, titleOrFilename(title, path), author});
  if (!saveToFile()) {
    LOG_ERR("BLOG", "Failed to persist book log entry: %s", path.c_str());
  }
}

bool BookLogStore::removeByPath(const std::string& path) {
  auto it = std::find_if(books.begin(), books.end(),
                         [&](const BookLogEntry& book) { return book.path == path; });
  if (it == books.end()) return false;

  books.erase(it);
  if (!saveToFile()) {
    LOG_ERR("BLOG", "Failed to persist removal from book log: %s", path.c_str());
  }
  return true;
}

void BookLogStore::updatePath(const std::string& oldPath, const std::string& newPath) {
  if (oldPath.empty() || newPath.empty() || oldPath == newPath) return;

  auto moved = std::find_if(books.begin(), books.end(),
                            [&](const BookLogEntry& book) { return book.path == oldPath; });
  if (moved == books.end()) return;

  moved->path = newPath;

  // If the destination path was already logged, keep the moved entry at its
  // current (normally newest) position and discard the duplicate.
  for (auto it = books.begin(); it != books.end();) {
    if (it != moved && it->path == newPath) {
      it = books.erase(it);
      // erase can invalidate moved when the erased element precedes it; find it
      // again before continuing.
      moved = std::find_if(books.begin(), books.end(),
                           [&](const BookLogEntry& book) { return book.path == newPath; });
    } else {
      ++it;
    }
  }

  if (!saveToFile()) {
    LOG_ERR("BLOG", "Failed to persist book log path update: %s -> %s", oldPath.c_str(), newPath.c_str());
  }
}

bool BookLogStore::isMissing(const BookLogEntry& book) { return !Storage.exists(book.path.c_str()); }

bool BookLogStore::saveToFile() const {
  JsonDocument doc;
  doc["version"] = BOOK_LOG_FILE_VERSION;
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : books) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = book.path;
    obj["title"] = book.title;
    obj["author"] = book.author;
  }

  String json;
  serializeJson(doc, json);
  Storage.mkdir("/.crosspoint");
  return Storage.writeFile(BOOK_LOG_FILE_JSON, json);
}

bool BookLogStore::loadFromFile(bool* fileFound) {
  const bool exists = Storage.exists(BOOK_LOG_FILE_JSON);
  if (fileFound) *fileFound = exists;
  if (!exists) return false;

  const String json = Storage.readFile(BOOK_LOG_FILE_JSON);
  if (json.length() == 0) {
    LOG_ERR("BLOG", "Book log file is empty or unreadable");
    return false;
  }

  JsonDocument doc;
  const auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("BLOG", "JSON parse error: %s", error.c_str());
    return false;
  }

  const uint8_t version = doc["version"] | 0;
  if (version != BOOK_LOG_FILE_VERSION) {
    LOG_ERR("BLOG", "Unsupported book log version: %u", version);
    return false;
  }

  JsonArray arr = doc["books"].as<JsonArray>();
  if (arr.isNull()) {
    LOG_ERR("BLOG", "Book log has no books array");
    return false;
  }

  std::vector<BookLogEntry> loaded;
  loaded.reserve(arr.size());
  std::unordered_set<std::string> seenPaths;
  for (JsonObject obj : arr) {
    const std::string path = obj["path"] | std::string("");
    if (path.empty() || !seenPaths.insert(path).second) continue;

    const std::string title = obj["title"] | std::string("");
    const std::string author = obj["author"] | std::string("");
    loaded.push_back({path, titleOrFilename(title, path), author});
  }

  books = std::move(loaded);
  LOG_DBG("BLOG", "Book log loaded from file (%d entries)", getCount());
  return true;
}

bool BookLogStore::initializeFromRecentBooks(const std::vector<RecentBook>& recentBooks) {
  books.clear();
  books.reserve(recentBooks.size());

  std::unordered_set<std::string> seenPaths;
  for (const auto& recent : recentBooks) {
    if (recent.path.empty() || !seenPaths.insert(recent.path).second) continue;
    books.push_back({recent.path, titleOrFilename(recent.title, recent.path), recent.author});
  }

  if (!saveToFile()) {
    LOG_ERR("BLOG", "Failed to initialize book log from recent books");
    return false;
  }

  LOG_DBG("BLOG", "Initialized book log from recents (%d entries)", getCount());
  return true;
}

bool BookLogStore::loadOrInitializeFromRecentBooks(const std::vector<RecentBook>& recentBooks) {
  bool fileFound = false;
  if (loadFromFile(&fileFound)) return true;
  if (fileFound) return false;
  return initializeFromRecentBooks(recentBooks);
}
