#pragma once

#include <cstdint>

#include "CrossPointSettings.h"

namespace BluetoothPageTurnerHid {

constexpr uint8_t KEYBOARD_REPORT_ID = 1;
constexpr uint8_t CONSUMER_REPORT_ID = 2;

constexpr uint8_t KEY_PAGE_UP = 0x4B;
constexpr uint8_t KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEY_LEFT_ARROW = 0x50;
constexpr uint8_t KEY_DOWN_ARROW = 0x51;
constexpr uint8_t KEY_UP_ARROW = 0x52;
constexpr uint16_t CONSUMER_VOLUME_UP = 0x00E9;
constexpr uint16_t CONSUMER_VOLUME_DOWN = 0x00EA;

enum class ReportKind { Keyboard, Consumer };

struct PageTurnReport {
  ReportKind kind;
  uint16_t usage;

  constexpr bool operator==(const PageTurnReport&) const = default;
};

struct ConsumerReport {
  uint8_t bytes[2];
};

constexpr ConsumerReport encodeConsumerUsage(uint16_t usage) {
  return {{static_cast<uint8_t>(usage), static_cast<uint8_t>(usage >> 8)}};
}

constexpr PageTurnReport resolvePageTurn(CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS profile, bool next) {
  switch (profile) {
    case CrossPointSettings::BT_KEYS_LEFT_RIGHT:
      return {ReportKind::Keyboard, next ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW};
    case CrossPointSettings::BT_KEYS_UP_DOWN:
      return {ReportKind::Keyboard, next ? KEY_DOWN_ARROW : KEY_UP_ARROW};
    case CrossPointSettings::BT_KEYS_VOLUME_UP_DOWN:
      return {ReportKind::Consumer, next ? CONSUMER_VOLUME_DOWN : CONSUMER_VOLUME_UP};
    case CrossPointSettings::BT_KEYS_PAGE_UP_DOWN:
    default:
      return {ReportKind::Keyboard, next ? KEY_PAGE_DOWN : KEY_PAGE_UP};
  }
}

}  // namespace BluetoothPageTurnerHid
