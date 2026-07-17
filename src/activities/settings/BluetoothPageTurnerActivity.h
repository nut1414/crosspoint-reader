#pragma once

#include <atomic>
#include <cstdint>

#include "activities/Activity.h"

class BLECharacteristic;
class BLEHIDDevice;
class BLEServer;

class BluetoothPageTurnerActivity final : public Activity {
  class ServerCallbacks;
  static constexpr uint32_t NO_PENDING_CONNECTION = UINT32_MAX;

  BLEHIDDevice* hidDevice = nullptr;
  BLECharacteristic* keyboardInput = nullptr;
  BLECharacteristic* consumerInput = nullptr;
  BLEServer* server = nullptr;
  ServerCallbacks* serverCallbacks = nullptr;

  volatile bool deviceConnected = false;
  volatile bool stoppingBle = false;
  volatile bool restartAdvertisingRequested = false;
  std::atomic<uint32_t> pendingConnectionHandle{NO_PENDING_CONNECTION};
  bool renderedConnected = false;
  bool bleStarted = false;
  bool bleAvailable = true;
  uint8_t lastBatteryLevel = 255;
  unsigned long lastBatteryLevelUpdateMs = 0;

  bool startBle();
  void stopBle();
  void setConnectedFromCallback(bool connected);
  void cycleKeyProfile();
  void disconnectAndAdvertise();
  void updateBatteryLevel(bool force);
  void sendPageTurn(bool next);
  void sendKeyboardKey(uint8_t usage);
  void sendConsumerKey(uint16_t usage);
  const char* getProfileLabel() const;
  const char* getStatusLabel() const;

 public:
  explicit BluetoothPageTurnerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BluetoothPageTurner", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return bleStarted; }
  bool preventPowerSaving() override { return false; }
};
