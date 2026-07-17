#include <gtest/gtest.h>

#include "CrossPointSettings.h"
#include "activities/settings/BluetoothPageTurnerHid.h"

namespace {

using BluetoothPageTurnerHid::PageTurnReport;
using BluetoothPageTurnerHid::ReportKind;

struct MappingCase {
  CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS profile;
  bool next;
  PageTurnReport expected;
};

}  // namespace

TEST(BluetoothPageTurnerHidTest, ResolvesEveryProfileAndDirection) {
  constexpr MappingCase cases[] = {
      {CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, false, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_PAGE_UP}},
      {CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, true, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_PAGE_DOWN}},
      {CrossPointSettings::BT_KEYS_LEFT_RIGHT, false, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_LEFT_ARROW}},
      {CrossPointSettings::BT_KEYS_LEFT_RIGHT, true, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_RIGHT_ARROW}},
      {CrossPointSettings::BT_KEYS_UP_DOWN, false, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_UP_ARROW}},
      {CrossPointSettings::BT_KEYS_UP_DOWN, true, {ReportKind::Keyboard, BluetoothPageTurnerHid::KEY_DOWN_ARROW}},
      {CrossPointSettings::BT_KEYS_VOLUME_UP_DOWN,
       false,
       {ReportKind::Consumer, BluetoothPageTurnerHid::CONSUMER_VOLUME_UP}},
      {CrossPointSettings::BT_KEYS_VOLUME_UP_DOWN,
       true,
       {ReportKind::Consumer, BluetoothPageTurnerHid::CONSUMER_VOLUME_DOWN}},
  };

  for (const auto& testCase : cases) {
    EXPECT_EQ(BluetoothPageTurnerHid::resolvePageTurn(testCase.profile, testCase.next), testCase.expected);
  }
}

TEST(BluetoothPageTurnerHidTest, AppendsVolumeProfileWithoutChangingSavedValues) {
  EXPECT_EQ(CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, 0);
  EXPECT_EQ(CrossPointSettings::BT_KEYS_LEFT_RIGHT, 1);
  EXPECT_EQ(CrossPointSettings::BT_KEYS_UP_DOWN, 2);
  EXPECT_EQ(CrossPointSettings::BT_KEYS_VOLUME_UP_DOWN, 3);
  EXPECT_EQ(CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS_COUNT, 4);
}

TEST(BluetoothPageTurnerHidTest, EncodesConsumerUsagesAsLittleEndianPressAndZeroRelease) {
  const auto volumeUp = BluetoothPageTurnerHid::encodeConsumerUsage(BluetoothPageTurnerHid::CONSUMER_VOLUME_UP);
  const auto volumeDown = BluetoothPageTurnerHid::encodeConsumerUsage(BluetoothPageTurnerHid::CONSUMER_VOLUME_DOWN);
  const auto release = BluetoothPageTurnerHid::encodeConsumerUsage(0);

  EXPECT_EQ(volumeUp.bytes[0], 0xE9);
  EXPECT_EQ(volumeUp.bytes[1], 0x00);
  EXPECT_EQ(volumeDown.bytes[0], 0xEA);
  EXPECT_EQ(volumeDown.bytes[1], 0x00);
  EXPECT_EQ(release.bytes[0], 0x00);
  EXPECT_EQ(release.bytes[1], 0x00);
}
