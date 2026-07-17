#include <gtest/gtest.h>

#include "CrossPointSettings.h"
#include "GfxRenderer.h"
#include "activities/settings/BluetoothPageTurnerInput.h"

CrossPointSettings CrossPointSettings::instance;

class BluetoothPageTurnerInputTest : public testing::Test {
 protected:
  HalGPIO gpio;
  GfxRenderer renderer;
  MappedInputManager input{gpio, renderer};

  void SetUp() override { SETTINGS.sideButtonLayout = CrossPointSettings::PREV_NEXT; }
};

TEST_F(BluetoothPageTurnerInputTest, ReaderDisabledLayoutKeepsBluetoothButtonsUsable) {
  SETTINGS.sideButtonLayout = CrossPointSettings::SIDE_BUTTONS_DISABLED;
  gpio.pressed[HalGPIO::BTN_UP] = true;

  EXPECT_FALSE(input.wasPressed(MappedInputManager::Button::PageBack));
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Previous);

  gpio.pressed[HalGPIO::BTN_UP] = false;
  gpio.pressed[HalGPIO::BTN_DOWN] = true;
  EXPECT_FALSE(input.wasPressed(MappedInputManager::Button::PageForward));
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Next);
}

TEST_F(BluetoothPageTurnerInputTest, DefaultLayoutMapsUpperToPreviousAndLowerToNext) {
  gpio.pressed[HalGPIO::BTN_UP] = true;
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Previous);

  gpio.pressed[HalGPIO::BTN_UP] = false;
  gpio.pressed[HalGPIO::BTN_DOWN] = true;
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Next);
}

TEST_F(BluetoothPageTurnerInputTest, SwappedLayoutReversesBluetoothButtons) {
  SETTINGS.sideButtonLayout = CrossPointSettings::NEXT_PREV;
  gpio.pressed[HalGPIO::BTN_UP] = true;
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Next);

  gpio.pressed[HalGPIO::BTN_UP] = false;
  gpio.pressed[HalGPIO::BTN_DOWN] = true;
  EXPECT_EQ(BluetoothPageTurnerInput::detectPageTurn(input), BluetoothPageTurnerInput::PageTurn::Previous);
}
