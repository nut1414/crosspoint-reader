#pragma once

#include <Arduino.h>

#include <string>
#include <unordered_map>

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

  void reset() {
    files.clear();
    writeCount = 0;
  }
  void setFile(const char* path, const std::string& content) { files[path] = String(content); }
  const String& getFile(const char* path) const { return files.at(path); }
  size_t getWriteCount() const { return writeCount; }
};

#define Storage HalStorage::getInstance()
