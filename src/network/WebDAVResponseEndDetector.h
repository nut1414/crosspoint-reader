#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Detects the closing WebDAV multistatus root without retaining response
// content. This lets an HTTP/1.0 identity response finish even when a server
// omits Content-Length and leaves the socket open.
class WebDAVResponseEndDetector {
 public:
  void consume(const uint8_t byte) noexcept {
    if (isComplete) return;

    switch (state) {
      case State::SEEK_TAG:
        if (byte == '<') state = State::AFTER_LT;
        break;
      case State::AFTER_LT:
        if (byte == '/') {
          nameLength = 0;
          nameOverflow = false;
          state = State::NAME;
        } else {
          state = byte == '<' ? State::AFTER_LT : State::SEEK_TAG;
        }
        break;
      case State::NAME:
        if (byte == '>') {
          finishTag();
        } else if (isWhitespace(byte)) {
          state = State::AFTER_NAME;
        } else if (byte == '<') {
          state = State::AFTER_LT;
        } else if (nameLength < sizeof(name)) {
          name[nameLength++] = static_cast<char>(byte);
        } else {
          nameOverflow = true;
        }
        break;
      case State::AFTER_NAME:
        if (byte == '>') {
          finishTag();
        } else if (!isWhitespace(byte)) {
          state = byte == '<' ? State::AFTER_LT : State::SEEK_TAG;
        }
        break;
    }
  }

  bool complete() const noexcept { return isComplete; }

 private:
  enum class State { SEEK_TAG, AFTER_LT, NAME, AFTER_NAME };

  static bool isWhitespace(const uint8_t byte) noexcept {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
  }

  void finishTag() noexcept {
    constexpr char ROOT_NAME[] = "multistatus";
    if (!nameOverflow && nameLength > 0) {
      size_t localNameStart = 0;
      for (size_t i = 0; i < nameLength; ++i) {
        if (name[i] == ':') localNameStart = i + 1;
      }
      const size_t localNameLength = nameLength - localNameStart;
      isComplete = localNameLength == sizeof(ROOT_NAME) - 1 &&
                   memcmp(name + localNameStart, ROOT_NAME, sizeof(ROOT_NAME) - 1) == 0;
    }
    state = State::SEEK_TAG;
  }

  State state = State::SEEK_TAG;
  char name[32]{};
  size_t nameLength = 0;
  bool nameOverflow = false;
  bool isComplete = false;
};
