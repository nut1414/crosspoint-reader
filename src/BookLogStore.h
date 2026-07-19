#pragma once

#include <string>
#include <vector>

struct RecentBook;

struct BookLogEntry {
  std::string path;
  std::string title;
  std::string author;

  bool operator==(const BookLogEntry& other) const { return path == other.path; }
};

class BookLogStore {
  static BookLogStore instance;

  std::vector<BookLogEntry> books;

 public:
  BookLogStore() = default;
  ~BookLogStore() = default;

  static BookLogStore& getInstance() { return instance; }

  // Add a book to the front of the log, refreshing its metadata if it was
  // already present. Book identity is the exact path.
  void recordBook(const std::string& path, const std::string& title, const std::string& author);

  // Remove one historical entry. Reopening the book will add it again.
  bool removeByPath(const std::string& path);

  // Repoint an entry after a firmware-managed file move. Keeps its position.
  void updatePath(const std::string& oldPath, const std::string& newPath);

  // Historical entries are deliberately retained when the backing file is gone.
  static bool isMissing(const BookLogEntry& book);

  const std::vector<BookLogEntry>& getBooks() const { return books; }
  int getCount() const { return static_cast<int>(books.size()); }

  bool saveToFile() const;

  // fileFound distinguishes a missing file (which should trigger migration)
  // from a present but unreadable/corrupt file (which must not be overwritten
  // during boot).
  bool loadFromFile(bool* fileFound = nullptr);

  // One-time migration used when book_log.json does not exist yet.
  bool initializeFromRecentBooks(const std::vector<RecentBook>& recentBooks);
  bool loadOrInitializeFromRecentBooks(const std::vector<RecentBook>& recentBooks);
};

#define BOOK_LOG BookLogStore::getInstance()
