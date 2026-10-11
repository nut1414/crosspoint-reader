#pragma once

#include <CrossPointSettings.h>

#include <cstdint>

namespace TextAntiAliasingPolicy {

constexpr bool usesGrayscale(const uint8_t mode) {
  return mode == CrossPointSettings::TEXT_AA_GRAYSCALE || mode == CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY;
}

constexpr bool usesDitheredRaster(const uint8_t mode) { return mode == CrossPointSettings::TEXT_AA_DITHERED; }

constexpr bool usesGrayscaleOnlyRefresh(const uint8_t mode) {
  return mode == CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY;
}

}  // namespace TextAntiAliasingPolicy
