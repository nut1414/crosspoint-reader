#include <gtest/gtest.h>

#include "activities/ActivityPowerPolicy.h"
#include "activities/settings/BluetoothPageTurnerPower.h"

TEST(ActivityPowerPolicyTest, KeepAwakeDoesNotRequireFullCpuSpeed) {
  const auto decision = ActivityPowerPolicy::decide(false, true, false);

  EXPECT_TRUE(decision.resetAutoSleepTimer);
  EXPECT_FALSE(decision.resetFullSpeedTimer);
}

TEST(ActivityPowerPolicyTest, UserInputWakesCpuAndResetsAutoSleep) {
  const auto decision = ActivityPowerPolicy::decide(true, false, false);

  EXPECT_TRUE(decision.resetAutoSleepTimer);
  EXPECT_TRUE(decision.resetFullSpeedTimer);
}

TEST(ActivityPowerPolicyTest, BackgroundWorkCanKeepCpuAtFullSpeed) {
  const auto decision = ActivityPowerPolicy::decide(false, true, true);

  EXPECT_TRUE(decision.resetAutoSleepTimer);
  EXPECT_TRUE(decision.resetFullSpeedTimer);
}

TEST(BluetoothPageTurnerPowerTest, UsesLowDutyConnectedParameters) {
  EXPECT_EQ(BluetoothPageTurnerPower::CONNECTION_INTERVAL_MIN, 24);
  EXPECT_EQ(BluetoothPageTurnerPower::CONNECTION_INTERVAL_MAX, 36);
  EXPECT_EQ(BluetoothPageTurnerPower::CONNECTION_LATENCY, 4);
  EXPECT_EQ(BluetoothPageTurnerPower::CONNECTION_SUPERVISION_TIMEOUT, 600);
  EXPECT_LE(BluetoothPageTurnerPower::maxIdleConnectionIntervalUs(), 250000U);
  EXPECT_TRUE(BluetoothPageTurnerPower::hasValidSupervisionTimeout());
}
