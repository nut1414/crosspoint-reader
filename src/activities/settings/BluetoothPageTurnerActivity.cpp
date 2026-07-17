#include "BluetoothPageTurnerActivity.h"

#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>
#include <sdkconfig.h>

#include <cstring>
#include <new>

#include "BluetoothPageTurnerHid.h"
#include "BluetoothPageTurnerInput.h"
#include "BluetoothPageTurnerPower.h"
#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr uint8_t HID_KEY_ENTER = 0x28;
constexpr uint16_t HID_KEYBOARD_APPEARANCE = 0x03C1;
constexpr unsigned long BATTERY_LEVEL_UPDATE_MS = 60000;

const uint8_t HID_REPORT_MAP[] = {
    // Keyboard report: modifiers, reserved byte, six key slots.
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xA1, 0x01,                    // Collection (Application)
    0x85, BluetoothPageTurnerHid::KEYBOARD_REPORT_ID,  //   Report ID
    0x05, 0x07,                    //   Usage Page (Keyboard)
    0x19, 0xE0,                    //   Usage Minimum (Left Control)
    0x29, 0xE7,                    //   Usage Maximum (Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)
    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x08,                    //   Report Size (8)
    0x81, 0x01,                    //   Input (Constant)
    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x65,                    //   Logical Maximum (101)
    0x05, 0x07,                    //   Usage Page (Keyboard)
    0x19, 0x00,                    //   Usage Minimum (Reserved)
    0x29, 0x65,                    //   Usage Maximum (Keyboard Application)
    0x81, 0x00,                    //   Input (Data, Array)
    0xC0,                          // End Collection

    // Consumer control report: one 16-bit usage (for example Volume Increment).
    0x05, 0x0C,                                      // Usage Page (Consumer)
    0x09, 0x01,                                      // Usage (Consumer Control)
    0xA1, 0x01,                                      // Collection (Application)
    0x85, BluetoothPageTurnerHid::CONSUMER_REPORT_ID,  //   Report ID
    0x15, 0x00,                                      //   Logical Minimum (0)
    0x26, 0xFF, 0x03,                                //   Logical Maximum (0x03FF)
    0x19, 0x00,                                      //   Usage Minimum (Unassigned)
    0x2A, 0xFF, 0x03,                                //   Usage Maximum (0x03FF)
    0x95, 0x01,                                      //   Report Count (1)
    0x75, 0x10,                                      //   Report Size (16)
    0x81, 0x00,                                      //   Input (Data, Array, Absolute)
    0xC0,                                            // End Collection
};

struct KeyboardReport {
  uint8_t modifiers = 0;
  uint8_t reserved = 0;
  uint8_t keys[6] = {};
} __attribute__((packed));
static_assert(sizeof(KeyboardReport) == 8);

class PageTurnerSecurityCallbacks final : public BLESecurityCallbacks {
 public:
  bool onSecurityRequest() override { return true; }
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t) override { return true; }
#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}
#endif
#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc*) override {}
#endif
};

PageTurnerSecurityCallbacks securityCallbacks;

}  // namespace

class BluetoothPageTurnerActivity::ServerCallbacks final : public BLEServerCallbacks {
  BluetoothPageTurnerActivity& owner;

 public:
  explicit ServerCallbacks(BluetoothPageTurnerActivity& owner) : owner(owner) {}

  void onConnect(BLEServer*) override { owner.setConnectedFromCallback(true); }

#if defined(CONFIG_NIMBLE_ENABLED)
  void onConnect(BLEServer*, ble_gap_conn_desc* desc) override {
    owner.pendingConnectionHandle.store(desc->conn_handle, std::memory_order_release);
  }

  void onConnParamsUpdate(uint16_t, uint16_t interval, uint16_t latency, uint16_t timeout, uint8_t status) override {
    LOG_DBG("BTPAGE", "Connection params: status=%u interval=%u latency=%u timeout=%u", status, interval, latency,
            timeout);
  }
#endif

  void onDisconnect(BLEServer*) override {
    owner.setConnectedFromCallback(false);
    if (!owner.stoppingBle) {
      owner.restartAdvertisingRequested = true;
    }
  }
};

void BluetoothPageTurnerActivity::onEnter() {
  Activity::onEnter();
  deviceConnected = false;
  restartAdvertisingRequested = false;
  pendingConnectionHandle.store(NO_PENDING_CONNECTION, std::memory_order_release);
  renderedConnected = false;
  bleAvailable = startBle();
  requestUpdate();
}

void BluetoothPageTurnerActivity::onExit() {
  stopBle();
  Activity::onExit();
}

bool BluetoothPageTurnerActivity::startBle() {
  WiFi.mode(WIFI_OFF);
  stoppingBle = false;

  if (!BLEDevice::init("CrossPoint PT")) {
    return false;
  }

  static BLESecurity security;
  (void)security;
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setAuthenticationMode(true, false, true);
  BLEDevice::setSecurityCallbacks(&securityCallbacks);

  server = BLEDevice::createServer();
  if (!server) {
    BLEDevice::deinit(false);
    return false;
  }
  serverCallbacks = new (std::nothrow) ServerCallbacks(*this);
  if (!serverCallbacks) {
    LOG_ERR("BTPAGE", "OOM creating BLE server callbacks");
    BLEDevice::deinit(false);
    server = nullptr;
    return false;
  }
  server->setCallbacks(serverCallbacks);

  hidDevice = new (std::nothrow) BLEHIDDevice(server);
  if (!hidDevice) {
    LOG_ERR("BTPAGE", "OOM creating BLE HID device");
    server->setCallbacks(nullptr);
    delete serverCallbacks;
    serverCallbacks = nullptr;
    BLEDevice::deinit(false);
    server = nullptr;
    return false;
  }
  hidDevice->manufacturer()->setValue("CrossPoint");
  hidDevice->pnp(0x02, 0x303A, 0x4001, 0x0100);
  hidDevice->hidInfo(0x00, 0x01);
  hidDevice->reportMap(const_cast<uint8_t*>(HID_REPORT_MAP), sizeof(HID_REPORT_MAP));
  keyboardInput = hidDevice->inputReport(BluetoothPageTurnerHid::KEYBOARD_REPORT_ID);
  consumerInput = hidDevice->inputReport(BluetoothPageTurnerHid::CONSUMER_REPORT_ID);
  lastBatteryLevel = 255;
  lastBatteryLevelUpdateMs = 0;
  updateBatteryLevel(true);
  hidDevice->startServices();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD_APPEARANCE);
  advertising->addServiceUUID(hidDevice->hidService()->getUUID());
  advertising->addServiceUUID(hidDevice->batteryService()->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(BluetoothPageTurnerPower::CONNECTION_INTERVAL_MIN);
  advertising->setMaxPreferred(BluetoothPageTurnerPower::CONNECTION_INTERVAL_MAX);
  BLEDevice::startAdvertising();

  bleStarted = true;
  return true;
}

void BluetoothPageTurnerActivity::stopBle() {
  stoppingBle = true;
  bleStarted = false;
  deviceConnected = false;
  restartAdvertisingRequested = false;
  pendingConnectionHandle.store(NO_PENDING_CONNECTION, std::memory_order_release);

  if (server) {
    server->setCallbacks(nullptr);
    if (server->getConnectedCount() > 0) {
      const auto peers = server->getPeerDevices(false);
      for (const auto& peer : peers) {
        server->disconnect(peer.first);
      }
      delay(100);
    }
  }

  delete serverCallbacks;
  delete hidDevice;
  serverCallbacks = nullptr;
  hidDevice = nullptr;
  keyboardInput = nullptr;
  consumerInput = nullptr;

  if (BLEDevice::getInitialized()) {
    BLEDevice::stopAdvertising();
    BLEDevice::setSecurityCallbacks(nullptr);
    BLEDevice::deinit(false);
  }

  server = nullptr;
  stoppingBle = false;
}

void BluetoothPageTurnerActivity::setConnectedFromCallback(bool connected) {
  deviceConnected = connected;
  if (!connected) {
    pendingConnectionHandle.store(NO_PENDING_CONNECTION, std::memory_order_release);
  }
}

void BluetoothPageTurnerActivity::cycleKeyProfile() {
  SETTINGS.bluetoothPageTurnerKeys =
      (SETTINGS.bluetoothPageTurnerKeys + 1) % CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS_COUNT;
  SETTINGS.saveToFile();
  requestUpdate();
}

void BluetoothPageTurnerActivity::disconnectAndAdvertise() {
  if (!bleStarted || !BLEDevice::getInitialized()) {
    return;
  }

  if (server && server->getConnectedCount() > 0) {
    const auto peers = server->getPeerDevices(false);
    for (const auto& peer : peers) {
      server->disconnect(peer.first);
    }
    deviceConnected = false;
    renderedConnected = false;
    restartAdvertisingRequested = false;
    delay(50);
  }

  updateBatteryLevel(true);
  BLEDevice::stopAdvertising();
  BLEDevice::startAdvertising();
  requestUpdate();
}

void BluetoothPageTurnerActivity::updateBatteryLevel(bool force) {
  if (!hidDevice) {
    return;
  }

  const uint16_t percentage = powerManager.getBatteryPercentage();
  const uint8_t level = percentage > 100 ? 100 : static_cast<uint8_t>(percentage);
  const unsigned long now = millis();
  if (!force && level == lastBatteryLevel) {
    lastBatteryLevelUpdateMs = now;
    return;
  }

  hidDevice->setBatteryLevel(level);
  lastBatteryLevel = level;
  lastBatteryLevelUpdateMs = now;
}

void BluetoothPageTurnerActivity::sendPageTurn(bool next) {
  if (!bleStarted || !deviceConnected || !server || server->getConnectedCount() == 0) {
    return;
  }

  const auto profile =
      static_cast<CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS>(SETTINGS.bluetoothPageTurnerKeys);
  const auto report = BluetoothPageTurnerHid::resolvePageTurn(profile, next);
  if (report.kind == BluetoothPageTurnerHid::ReportKind::Consumer) {
    sendConsumerKey(report.usage);
  } else {
    sendKeyboardKey(static_cast<uint8_t>(report.usage));
  }
}

void BluetoothPageTurnerActivity::sendKeyboardKey(uint8_t usage) {
  if (!keyboardInput || !server || server->getConnectedCount() == 0) {
    return;
  }

  KeyboardReport report;
  report.keys[0] = usage;
  keyboardInput->setValue(reinterpret_cast<const uint8_t*>(&report), sizeof(report));
  keyboardInput->notify();

  delay(8);
  if (!server || server->getConnectedCount() == 0) {
    return;
  }

  KeyboardReport releaseReport;
  keyboardInput->setValue(reinterpret_cast<const uint8_t*>(&releaseReport), sizeof(releaseReport));
  keyboardInput->notify();
}

void BluetoothPageTurnerActivity::sendConsumerKey(uint16_t usage) {
  if (!consumerInput || !server || server->getConnectedCount() == 0) {
    return;
  }

  const auto report = BluetoothPageTurnerHid::encodeConsumerUsage(usage);
  consumerInput->setValue(report.bytes, sizeof(report.bytes));
  consumerInput->notify();

  delay(8);
  if (!server || server->getConnectedCount() == 0) {
    return;
  }

  const auto releaseReport = BluetoothPageTurnerHid::encodeConsumerUsage(0);
  consumerInput->setValue(releaseReport.bytes, sizeof(releaseReport.bytes));
  consumerInput->notify();
}

const char* BluetoothPageTurnerActivity::getProfileLabel() const {
  switch (static_cast<CrossPointSettings::BLUETOOTH_PAGE_TURNER_KEYS>(SETTINGS.bluetoothPageTurnerKeys)) {
    case CrossPointSettings::BT_KEYS_LEFT_RIGHT:
      return tr(STR_BT_KEYS_LEFT_RIGHT);
    case CrossPointSettings::BT_KEYS_UP_DOWN:
      return tr(STR_BT_KEYS_UP_DOWN);
    case CrossPointSettings::BT_KEYS_VOLUME_UP_DOWN:
      return tr(STR_BT_KEYS_VOLUME_UP_DOWN);
    case CrossPointSettings::BT_KEYS_PAGE_UP_DOWN:
    default:
      return tr(STR_BT_KEYS_PAGE_UP_DOWN);
  }
}

const char* BluetoothPageTurnerActivity::getStatusLabel() const {
  if (!bleAvailable) {
    return tr(STR_BT_STATUS_UNAVAILABLE);
  }
  if (deviceConnected) {
    return tr(STR_BT_STATUS_CONNECTED);
  }
  return tr(STR_BT_STATUS_ADVERTISING);
}

void BluetoothPageTurnerActivity::loop() {
#if defined(CONFIG_NIMBLE_ENABLED)
  const uint32_t pendingHandle = pendingConnectionHandle.exchange(NO_PENDING_CONNECTION, std::memory_order_acquire);
  if (pendingHandle != NO_PENDING_CONNECTION && deviceConnected && server) {
    if (!server->requestConnParams(
            static_cast<uint16_t>(pendingHandle), BluetoothPageTurnerPower::CONNECTION_INTERVAL_MIN,
            BluetoothPageTurnerPower::CONNECTION_INTERVAL_MAX, BluetoothPageTurnerPower::CONNECTION_LATENCY,
            BluetoothPageTurnerPower::CONNECTION_SUPERVISION_TIMEOUT)) {
      LOG_DBG("BTPAGE", "Could not start low-power connection parameter request");
    }
  }
#endif

  if (restartAdvertisingRequested && bleStarted && !stoppingBle && BLEDevice::getInitialized()) {
    restartAdvertisingRequested = false;
    BLEDevice::startAdvertising();
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    cycleKeyProfile();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    disconnectAndAdvertise();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    sendKeyboardKey(HID_KEY_ENTER);
    return;
  }

  const bool connected = deviceConnected;
  if (connected != renderedConnected) {
    renderedConnected = connected;
    if (connected) {
      updateBatteryLevel(true);
    }
    requestUpdate();
  }

  if (bleStarted && hidDevice) {
    const unsigned long now = millis();
    if (lastBatteryLevelUpdateMs == 0 || now - lastBatteryLevelUpdateMs >= BATTERY_LEVEL_UPDATE_MS) {
      updateBatteryLevel(false);
    }
  }

  switch (BluetoothPageTurnerInput::detectPageTurn(mappedInput)) {
    case BluetoothPageTurnerInput::PageTurn::Previous:
      sendPageTurn(false);
      break;
    case BluetoothPageTurnerInput::PageTurn::Next:
      sendPageTurn(true);
      break;
    case BluetoothPageTurnerInput::PageTurn::None:
      break;
  }
}

void BluetoothPageTurnerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_BLUETOOTH_PAGE_TURNER));

  renderer.drawCenteredText(UI_12_FONT_ID, contentTop, getStatusLabel(), true, EpdFontFamily::BOLD);

  int y = contentTop + lineHeight * 3;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_BT_DEVICE_LABEL));
  y += lineHeight + metrics.verticalSpacing;

  std::string profile = tr(STR_BT_KEYS_LABEL);
  profile += getProfileLabel();
  renderer.drawCenteredText(UI_10_FONT_ID, y, profile.c_str());

  const auto labels =
      mappedInput.mapLabels(tr(STR_EXIT), tr(STR_BT_KEYS_HINT), tr(STR_BT_REPAIR_HINT), tr(STR_BT_ENTER_HINT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  const bool swapped = SETTINGS.sideButtonLayout == CrossPointSettings::NEXT_PREV;
  GUI.drawSideButtonHints(renderer, swapped ? tr(STR_BT_NEXT) : tr(STR_BT_PREVIOUS),
                          swapped ? tr(STR_BT_PREVIOUS) : tr(STR_BT_NEXT));

  renderer.displayBuffer();
}
