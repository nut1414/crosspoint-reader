#pragma once

#include <Arduino.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_map>

class HalFile {
 public:
  void open(const std::string& contents) {
    contents_ = contents;
    position_ = 0;
  }

  size_t write(const uint8_t* buffer, const size_t length) {
    contents_.replace(position_, length, reinterpret_cast<const char*>(buffer), length);
    position_ += length;
    return length;
  }

  int available() const { return static_cast<int>(contents_.size() - position_); }

  int read(uint8_t* buffer, const size_t length) { return read(reinterpret_cast<char*>(buffer), length); }

  int read(char* buffer, const size_t length) {
    const size_t count = std::min(length, contents_.size() - position_);
    if (count == 0) return 0;
    std::memcpy(buffer, contents_.data() + position_, count);
    position_ += count;
    return static_cast<int>(count);
  }

 private:
  std::string contents_;
  size_t position_ = 0;
};

class HalStorage {
 private:
  std::unordered_map<std::string, String> files;
  size_t writeCount = 0;

 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool exists(const char* path) const { return files.find(path) != files.end(); }
  String readFile(const char* path) const {
    const auto it = files.find(path);
    return it == files.end() ? String() : it->second;
  }
  bool writeFile(const char* path, const String& content) {
    files[path] = content;
    ++writeCount;
    return true;
  }
  bool mkdir(const char*) { return true; }
  bool rename(const char* oldPath, const char* newPath) {
    const auto it = files.find(oldPath);
    if (it == files.end()) return false;
    files[newPath] = it->second;
    files.erase(it);
    return true;
  }
  bool openFileForRead(const char*, const char* path, HalFile& file) const {
    const auto it = files.find(path);
    if (it == files.end()) return false;
    file.open(it->second);
    return true;
  }

  void reset() {
    files.clear();
    writeCount = 0;
  }
  void setFile(const char* path, const std::string& content) { files[path] = String(content); }
  const String& getFile(const char* path) const { return files.at(path); }
  size_t getWriteCount() const { return writeCount; }
};

#define Storage HalStorage::getInstance()
