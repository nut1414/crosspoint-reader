#include "WifiSelectionPolicy.h"

#include <algorithm>

#include "WifiCredentialStore.h"

namespace {
const WifiCredential* findCredential(const std::vector<WifiCredential>& credentials, const std::string& ssid) {
  const auto credential = std::find_if(credentials.begin(), credentials.end(),
                                       [&ssid](const WifiCredential& item) { return item.ssid == ssid; });
  return credential == credentials.end() ? nullptr : &*credential;
}
}  // namespace

int findSingleUsableSavedNetwork(const std::vector<WifiNetworkInfo>& networks,
                                 const std::vector<WifiCredential>& credentials) {
  int candidateIndex = -1;

  for (size_t i = 0; i < networks.size(); i++) {
    const auto& network = networks[i];
    const auto* credential = findCredential(credentials, network.ssid);
    if (!credential || (network.isEncrypted && credential->password.empty())) continue;

    bool duplicate = false;
    for (size_t previous = 0; previous < i; previous++) {
      if (networks[previous].ssid == network.ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    if (candidateIndex >= 0) return -1;
    candidateIndex = static_cast<int>(i);
  }

  return candidateIndex;
}
