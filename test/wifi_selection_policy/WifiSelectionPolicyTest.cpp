#include <gtest/gtest.h>

#include "KOReaderCredentialStore.h"
#include "WifiCredentialStore.h"
#include "WifiSelectionPolicy.h"

namespace {
WifiNetworkInfo network(const char* ssid, const bool encrypted) { return {ssid, -50, encrypted, true}; }
}  // namespace

TEST(WifiSelectionPolicyTest, FindsOnlyUsableSavedNetwork) {
  const std::vector<WifiNetworkInfo> networks = {network("Cafe", false), network("Home", true)};
  const std::vector<WifiCredential> credentials = {{"Home", "secret"}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), 1);
}

TEST(WifiSelectionPolicyTest, ShowsListWhenNoSavedNetworkIsVisible) {
  const std::vector<WifiNetworkInfo> networks = {network("Cafe", false)};
  const std::vector<WifiCredential> credentials = {{"Home", "secret"}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), -1);
}

TEST(WifiSelectionPolicyTest, RequiresUnambiguousSavedNetwork) {
  const std::vector<WifiNetworkInfo> networks = {network("Home", true), network("Hotspot", true)};
  const std::vector<WifiCredential> credentials = {{"Home", "home-pass"}, {"Hotspot", "hotspot-pass"}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), -1);
}

TEST(WifiSelectionPolicyTest, IgnoresDuplicateAccessPointsForSameSsid) {
  const std::vector<WifiNetworkInfo> networks = {network("Home", true), network("Home", true)};
  const std::vector<WifiCredential> credentials = {{"Home", "secret"}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), 0);
}

TEST(WifiSelectionPolicyTest, RejectsEncryptedNetworkWithoutPassword) {
  const std::vector<WifiNetworkInfo> networks = {network("Home", true)};
  const std::vector<WifiCredential> credentials = {{"Home", ""}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), -1);
}

TEST(WifiSelectionPolicyTest, AllowsSavedOpenNetworkWithoutPassword) {
  const std::vector<WifiNetworkInfo> networks = {network("Cafe", false)};
  const std::vector<WifiCredential> credentials = {{"Cafe", ""}};

  EXPECT_EQ(findSingleUsableSavedNetwork(networks, credentials), 0);
}

TEST(WifiSelectionPolicyTest, AppliesEachConnectionPolicyAtTheExpectedStage) {
  EXPECT_FALSE(shouldTryLastNetworkOnEntry(WifiConnectionPolicy::SMART));
  EXPECT_TRUE(shouldAutoConnectAfterScan(WifiConnectionPolicy::SMART));

  EXPECT_FALSE(shouldTryLastNetworkOnEntry(WifiConnectionPolicy::CHOOSE_EVERY_TIME));
  EXPECT_FALSE(shouldAutoConnectAfterScan(WifiConnectionPolicy::CHOOSE_EVERY_TIME));

  EXPECT_TRUE(shouldTryLastNetworkOnEntry(WifiConnectionPolicy::AUTO_CONNECT_LAST));
  EXPECT_FALSE(shouldAutoConnectAfterScan(WifiConnectionPolicy::AUTO_CONNECT_LAST));
}

TEST(KOReaderWifiConnectionModeTest, PreservesValidValuesAndDefaultsInvalidValuesToSmart) {
  EXPECT_EQ(normalizeKOReaderWifiConnectionMode(0), KOReaderWifiConnectionMode::SMART);
  EXPECT_EQ(normalizeKOReaderWifiConnectionMode(1), KOReaderWifiConnectionMode::CHOOSE_EVERY_TIME);
  EXPECT_EQ(normalizeKOReaderWifiConnectionMode(2), KOReaderWifiConnectionMode::AUTO_CONNECT_LAST);
  EXPECT_EQ(normalizeKOReaderWifiConnectionMode(3), KOReaderWifiConnectionMode::SMART);
  EXPECT_EQ(normalizeKOReaderWifiConnectionMode(255), KOReaderWifiConnectionMode::SMART);
}
