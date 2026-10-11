#pragma once

#include <string>
#include <utility>

class String : public std::string {
 public:
  using std::string::string;
  using std::string::operator=;

  String() = default;
  String(const std::string& value) : std::string(value) {}
  String(std::string&& value) : std::string(std::move(value)) {}

  String& append(const char* value) {
    std::string::append(value);
    return *this;
  }

  String& append(const char* value, std::size_t length) {
    std::string::append(value, length);
    return *this;
  }

  bool isEmpty() const { return empty(); }
};
