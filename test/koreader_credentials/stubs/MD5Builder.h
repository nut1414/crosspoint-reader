#pragma once

#include <Arduino.h>

// Credential-store tests do not exercise password hashing.
class MD5Builder {
 public:
  void begin() {}
  void add(const char*) {}
  void calculate() {}
  String toString() const { return "unused-md5-stub"; }
};
