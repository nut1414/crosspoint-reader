#pragma once

#include <cstdint>

namespace coverageDither {

// Coverage uses the same four levels as 2-bit fonts:
// 0 = white, 1 = light gray, 2 = dark gray, 3 = black.
inline constexpr bool isBlack(const uint8_t coverage, const int x, const int y) {
  if (coverage == 0) return false;
  if (coverage >= 3) return true;

  constexpr uint8_t BAYER_4X4[16] = {
      0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5,
  };
  const uint8_t threshold = BAYER_4X4[(static_cast<uint32_t>(y) & 3U) * 4U + (static_cast<uint32_t>(x) & 3U)];
  // Rounded thirds across a 16-pixel tile: 5/16 for light gray and
  // 11/16 for dark gray.
  const uint8_t blackPixels = static_cast<uint8_t>((coverage * 16U + 1U) / 3U);
  return threshold < blackPixels;
}

}  // namespace coverageDither
