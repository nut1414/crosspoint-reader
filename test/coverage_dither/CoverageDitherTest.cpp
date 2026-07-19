#include <gtest/gtest.h>

#include "lib/GfxRenderer/CoverageDither.h"

TEST(CoverageDither, KeepsCoverageExtremesSolid) {
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      EXPECT_FALSE(coverageDither::isBlack(0, x, y));
      EXPECT_TRUE(coverageDither::isBlack(3, x, y));
    }
  }
}

TEST(CoverageDither, ApproximatesIntermediateCoverageAcrossOneTile) {
  int lightPixels = 0;
  int darkPixels = 0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      lightPixels += coverageDither::isBlack(1, x, y) ? 1 : 0;
      darkPixels += coverageDither::isBlack(2, x, y) ? 1 : 0;
    }
  }

  EXPECT_EQ(lightPixels, 5);
  EXPECT_EQ(darkPixels, 11);
}

TEST(CoverageDither, IsDeterministicAndScreenAnchored) {
  for (uint8_t coverage = 0; coverage <= 3; ++coverage) {
    for (int y = -4; y < 8; ++y) {
      for (int x = -4; x < 8; ++x) {
        const bool expected = coverageDither::isBlack(coverage, x, y);
        EXPECT_EQ(coverageDither::isBlack(coverage, x, y), expected);
        EXPECT_EQ(coverageDither::isBlack(coverage, x + 4, y), expected);
        EXPECT_EQ(coverageDither::isBlack(coverage, x, y + 4), expected);
      }
    }
  }
}
