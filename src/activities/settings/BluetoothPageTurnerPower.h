#pragma once

#include <cstdint>

namespace BluetoothPageTurnerPower {

// BLE connection intervals use 1.25 ms units; supervision timeout uses 10 ms units.
constexpr uint16_t CONNECTION_INTERVAL_MIN = 24;  // 30 ms
constexpr uint16_t CONNECTION_INTERVAL_MAX = 36;  // 45 ms
constexpr uint16_t CONNECTION_LATENCY = 4;
constexpr uint16_t CONNECTION_SUPERVISION_TIMEOUT = 600;  // 6 s

constexpr uint32_t maxIdleConnectionIntervalUs() {
  return static_cast<uint32_t>(CONNECTION_INTERVAL_MAX) * (CONNECTION_LATENCY + 1) * 1250;
}

constexpr bool hasValidSupervisionTimeout() {
  const uint32_t supervisionTimeoutUs = static_cast<uint32_t>(CONNECTION_SUPERVISION_TIMEOUT) * 10000;
  return supervisionTimeoutUs > 2 * maxIdleConnectionIntervalUs();
}

static_assert(hasValidSupervisionTimeout());

}  // namespace BluetoothPageTurnerPower
