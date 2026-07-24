#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

// Allocation-free decoder for the HTTP/1.1 chunk framing exposed by
// HTTPClient::getStreamPtr(). The caller still owns transport timeouts and
// writes bytes reported as DATA to its destination.
class HttpChunkDecoder {
 public:
  enum class Result { CONTINUE, DATA, COMPLETE, ERROR };

  Result consume(const uint8_t byte) noexcept {
    switch (state) {
      case State::SIZE:
        return consumeSize(byte);
      case State::SIZE_EXTENSION:
        if (byte == '\r') state = State::SIZE_LF;
        return byte == '\n' ? fail() : Result::CONTINUE;
      case State::SIZE_LF:
        if (byte != '\n') return fail();
        if (chunkSize == 0) {
          state = State::COMPLETE;
          return Result::COMPLETE;
        }
        remaining = chunkSize;
        state = State::DATA;
        return Result::CONTINUE;
      case State::DATA:
        if (--remaining == 0) state = State::DATA_CR;
        return Result::DATA;
      case State::DATA_CR:
        if (byte != '\r') return fail();
        state = State::DATA_LF;
        return Result::CONTINUE;
      case State::DATA_LF:
        if (byte != '\n') return fail();
        chunkSize = 0;
        hasSizeDigit = false;
        state = State::SIZE;
        return Result::CONTINUE;
      case State::COMPLETE:
        return Result::COMPLETE;
      case State::ERROR:
        return Result::ERROR;
    }
    return fail();
  }

  bool complete() const noexcept { return state == State::COMPLETE; }

 private:
  enum class State { SIZE, SIZE_EXTENSION, SIZE_LF, DATA, DATA_CR, DATA_LF, COMPLETE, ERROR };

  static int hexValue(const uint8_t byte) noexcept {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
  }

  Result consumeSize(const uint8_t byte) noexcept {
    const int digit = hexValue(byte);
    if (digit >= 0) {
      if (chunkSize > (std::numeric_limits<size_t>::max() - static_cast<size_t>(digit)) / 16) {
        return fail();
      }
      chunkSize = chunkSize * 16 + static_cast<size_t>(digit);
      hasSizeDigit = true;
      return Result::CONTINUE;
    }
    if (!hasSizeDigit) return fail();
    if (byte == ';' || byte == ' ' || byte == '\t') {
      state = State::SIZE_EXTENSION;
      return Result::CONTINUE;
    }
    if (byte == '\r') {
      state = State::SIZE_LF;
      return Result::CONTINUE;
    }
    return fail();
  }

  Result fail() noexcept {
    state = State::ERROR;
    return Result::ERROR;
  }

  State state = State::SIZE;
  size_t chunkSize = 0;
  size_t remaining = 0;
  bool hasSizeDigit = false;
};
