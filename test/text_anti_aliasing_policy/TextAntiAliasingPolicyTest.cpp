#include <gtest/gtest.h>

#include "CrossPointSettings.h"
#include "activities/reader/TextAntiAliasingPolicy.h"

TEST(TextAntiAliasingPolicyTest, PreservesExistingPersistedValuesAndAppendsGrayscaleOnly) {
  EXPECT_EQ(CrossPointSettings::TEXT_AA_OFF, 0);
  EXPECT_EQ(CrossPointSettings::TEXT_AA_GRAYSCALE, 1);
  EXPECT_EQ(CrossPointSettings::TEXT_AA_DITHERED, 2);
  EXPECT_EQ(CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY, 3);
  EXPECT_EQ(CrossPointSettings::TEXT_AA_MODE_COUNT, 4);
}

TEST(TextAntiAliasingPolicyTest, GrayscaleOnlyUsesTheGrayscalePassWithoutDitheredRasterization) {
  EXPECT_TRUE(TextAntiAliasingPolicy::usesGrayscale(CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY));
  EXPECT_TRUE(TextAntiAliasingPolicy::usesGrayscaleOnlyRefresh(CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY));
  EXPECT_FALSE(TextAntiAliasingPolicy::usesDitheredRaster(CrossPointSettings::TEXT_AA_GRAYSCALE_ONLY));
}

TEST(TextAntiAliasingPolicyTest, ExistingModesKeepTheirRefreshBehavior) {
  EXPECT_TRUE(TextAntiAliasingPolicy::usesGrayscale(CrossPointSettings::TEXT_AA_GRAYSCALE));
  EXPECT_FALSE(TextAntiAliasingPolicy::usesGrayscaleOnlyRefresh(CrossPointSettings::TEXT_AA_GRAYSCALE));
  EXPECT_TRUE(TextAntiAliasingPolicy::usesDitheredRaster(CrossPointSettings::TEXT_AA_DITHERED));
  EXPECT_FALSE(TextAntiAliasingPolicy::usesGrayscale(CrossPointSettings::TEXT_AA_DITHERED));
  EXPECT_FALSE(TextAntiAliasingPolicy::usesGrayscale(CrossPointSettings::TEXT_AA_OFF));
}
