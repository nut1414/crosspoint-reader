#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct WifiCredential;

enum class WifiConnectionPolicy : uint8_t {
  SMART,
  CHOOSE_EVERY_TIME,
  AUTO_CONNECT_LAST,
};

constexpr bool shouldTryLastNetworkOnEntry(const WifiConnectionPolicy policy) {
  return policy == WifiConnectionPolicy::AUTO_CONNECT_LAST;
}

constexpr bool shouldAutoConnectAfterScan(const WifiConnectionPolicy policy) {
  return policy == WifiConnectionPolicy::SMART;
}

struct WifiNetworkInfo {
  std::string ssid;
  int32_t rssi;
  bool isEncrypted;
  bool hasSavedPassword;
};

// Returns the index of the sole unique visible network with usable saved
// credentials. Returns -1 when there is no unambiguous candidate.
int findSingleUsableSavedNetwork(const std::vector<WifiNetworkInfo>& networks,
                                 const std::vector<WifiCredential>& credentials);
