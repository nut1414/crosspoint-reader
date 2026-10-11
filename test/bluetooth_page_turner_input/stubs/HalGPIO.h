#pragma once

#include <Arduino.h>

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
  bool released[7] = {};

  void update() const {}
  bool wasPressed(uint8_t button) const { return pressed[button]; }
  bool wasReleased(uint8_t button) const { return released[button]; }
  bool isPressed(uint8_t button) const { return pressed[button]; }
  bool wasAnyPressed() const {
    for (const bool value : pressed) {
      if (value) return true;
    }
    return false;
  }
  bool wasAnyReleased() const {
    for (const bool value : released) {
      if (value) return true;
    }
    return false;
  }
  unsigned long getHeldTime() const { return 0; }
  bool hasTouch() const { return false; }
  bool hasHomeKey() const { return false; }
  bool wasHomeKeyTapped() const { return false; }
  bool wasHomeKeyLongPressed() const { return false; }
  bool wasHomeKeyPressed() const { return false; }
  bool wasTouchTap(float&, float&) const { return false; }
  bool isTouchTapCandidate(float&, float&, unsigned long&) const { return false; }
  bool wasTouchLongPress(float&, float&) const { return false; }
  void suppressTouchContact() {}
  bool isTouchHeldAt(float&, float&) const { return false; }
  bool wasTouchReleased() const { return false; }
  bool wasSwipe(float&, float&, float&, float&) const { return false; }
  unsigned long lastTouchHeldMs() const { return 0; }
  unsigned long getPowerButtonHeldTime() const { return 0; }
};
