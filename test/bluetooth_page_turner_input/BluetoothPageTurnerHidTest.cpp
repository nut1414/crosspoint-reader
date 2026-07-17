#include <gtest/gtest.h>

#include "CrossPointSettings.h"
#include "activities/settings/BluetoothPageTurnerHid.h"

namespace {

struct MappingCase {
  CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS profile;
  bool next;
  uint8_t expected;
};

}  // namespace

TEST(BluetoothPageTurnerHidTest, ResolvesEveryProfileAndDirection) {
  constexpr MappingCase cases[] = {
      {CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, false, BluetoothPageTurnerHid::KEY_PAGE_UP},
      {CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, true, BluetoothPageTurnerHid::KEY_PAGE_DOWN},
      {CrossPointSettings::BT_KEYS_LEFT_RIGHT, false, BluetoothPageTurnerHid::KEY_LEFT_ARROW},
      {CrossPointSettings::BT_KEYS_LEFT_RIGHT, true, BluetoothPageTurnerHid::KEY_RIGHT_ARROW},
      {CrossPointSettings::BT_KEYS_UP_DOWN, false, BluetoothPageTurnerHid::KEY_UP_ARROW},
      {CrossPointSettings::BT_KEYS_UP_DOWN, true, BluetoothPageTurnerHid::KEY_DOWN_ARROW},
  };

  for (const auto& testCase : cases) {
    EXPECT_EQ(BluetoothPageTurnerHid::resolvePageTurn(testCase.profile, testCase.next), testCase.expected);
  }
}

TEST(BluetoothPageTurnerHidTest, KeepsExistingProfileValuesAndCount) {
  EXPECT_EQ(CrossPointSettings::BT_KEYS_PAGE_UP_DOWN, 0);
  EXPECT_EQ(CrossPointSettings::BT_KEYS_LEFT_RIGHT, 1);
  EXPECT_EQ(CrossPointSettings::BT_KEYS_UP_DOWN, 2);
  EXPECT_EQ(CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS_COUNT, 3);
}
