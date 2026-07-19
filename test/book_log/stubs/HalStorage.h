#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

using String = std::string;

class HalStorage {
  static HalStorage instance;

  std::unordered_map<std::string, String> files;
  std::unordered_set<std::string> existingPaths;

 public:
  static HalStorage& getInstance() { return instance; }

  bool exists(const char* path) const {
    return files.find(path) != files.end() || existingPaths.find(path) != existingPaths.end();
  }

  String readFile(const char* path) const {
    const auto it = files.find(path);
    return it == files.end() ? String() : it->second;
  }

  bool writeFile(const char* path, const String& content) {
    files[path] = content;
    return true;
  }

  bool mkdir(const char*) { return true; }

  void reset() {
    files.clear();
    existingPaths.clear();
  }

  void setFile(const std::string& path, const String& content) { files[path] = content; }
  const String& getFile(const std::string& path) const { return files.at(path); }
  void markPathExisting(const std::string& path) { existingPaths.insert(path); }
  void markPathMissing(const std::string& path) { existingPaths.erase(path); }
};

#define Storage HalStorage::getInstance()
