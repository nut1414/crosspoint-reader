#pragma once

#include <cstdint>

class HalGPIO {
 public:
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;

  bool pressed[7] = {};

  void update() const {}
  bool wasPressed(uint8_t button) const { return pressed[button]; }
  bool wasReleased(uint8_t) const { return false; }
  bool isPressed(uint8_t button) const { return pressed[button]; }
  bool wasAnyPressed() const { return false; }
  bool wasAnyReleased() const { return false; }
  unsigned long getHeldTime() const { return 0; }
};
