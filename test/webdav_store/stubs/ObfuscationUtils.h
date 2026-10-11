#pragma once

#include <Arduino.h>

#include <cstddef>
#include <string>

namespace obfuscation {

String obfuscateToBase64(const std::string& plaintext);
std::string deobfuscateFromBase64(const char* encoded, bool* ok = nullptr);
std::string deobfuscateFromBase64(const char* encoded, size_t maxDecodedLength, bool* ok, bool* tooLong);

}  // namespace obfuscation
