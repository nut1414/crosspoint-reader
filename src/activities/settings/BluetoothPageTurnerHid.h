#pragma once

#include <cstdint>

#include "CrossPointSettings.h"

namespace BluetoothPageTurnerHid {

constexpr uint8_t KEYBOARD_REPORT_ID = 1;

constexpr uint8_t KEY_PAGE_UP = 0x4B;
constexpr uint8_t KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEY_LEFT_ARROW = 0x50;
constexpr uint8_t KEY_DOWN_ARROW = 0x51;
constexpr uint8_t KEY_UP_ARROW = 0x52;

constexpr uint8_t resolvePageTurn(CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS profile, bool next) {
  switch (profile) {
    case CrossPointSettings::BT_KEYS_LEFT_RIGHT:
      return next ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW;
    case CrossPointSettings::BT_KEYS_UP_DOWN:
      return next ? KEY_DOWN_ARROW : KEY_UP_ARROW;
    case CrossPointSettings::BT_KEYS_PAGE_UP_DOWN:
    default:
      return next ? KEY_PAGE_DOWN : KEY_PAGE_UP;
  }
}

}  // namespace BluetoothPageTurnerHid
