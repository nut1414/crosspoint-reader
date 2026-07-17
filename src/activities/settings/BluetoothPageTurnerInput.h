#pragma once

#include "CrossPointSettings.h"
#include "MappedInputManager.h"

namespace BluetoothPageTurnerInput {

enum class PageTurn { None, Previous, Next };

inline PageTurn detectPageTurn(const MappedInputManager& input) {
  // The Disabled layout applies to reader navigation only. This activity is itself a remote,
  // so its physical side buttons stay active and use the default order when not explicitly swapped.
  const bool upperPressed = input.wasPressed(MappedInputManager::Button::Up);
  const bool lowerPressed = input.wasPressed(MappedInputManager::Button::Down);
  const bool swapped = SETTINGS.sideButtonLayout == CrossPointSettings::NEXT_PREV;

  const bool previousPressed = swapped ? lowerPressed : upperPressed;
  const bool nextPressed = swapped ? upperPressed : lowerPressed;

  if (previousPressed) {
    return PageTurn::Previous;
  }
  if (nextPressed) {
    return PageTurn::Next;
  }
  return PageTurn::None;
}

}  // namespace BluetoothPageTurnerInput
