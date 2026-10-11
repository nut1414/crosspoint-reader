#include <ArduinoJson.h>
#include <HalStorage.h>
#include <ObfuscationUtils.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "KOReaderCredentialStore.h"

namespace obfuscation {
namespace {
constexpr char TEST_PREFIX[] = "test-obfuscated:";

std::string decode(const char* encoded, bool* ok) {
  if (ok) *ok = false;
  if (!encoded || std::string(encoded).compare(0, sizeof(TEST_PREFIX) - 1, TEST_PREFIX) != 0) return "";
  if (ok) *ok = true;
  return encoded + sizeof(TEST_PREFIX) - 1;
}
}  // namespace

String obfuscateToBase64(const std::string& plaintext) {
  if (plaintext.empty()) return "";
  return String(std::string(TEST_PREFIX) + plaintext);
}

std::string deobfuscateFromBase64(const char* encoded, bool* ok) { return decode(encoded, ok); }

std::string deobfuscateFromBase64(const char* encoded, const size_t maxDecodedLength, bool* ok, bool* tooLong) {
  if (tooLong) *tooLong = false;
  std::string result = decode(encoded, ok);
  if (ok && !*ok) return "";
  if (result.size() > maxDecodedLength) {
    if (ok) *ok = false;
    if (tooLong) *tooLong = true;
    return "";
  }
  return result;
}
}  // namespace obfuscation

namespace {
constexpr char JSON_PATH[] = "/.crosspoint/koreader.json";
constexpr char BINARY_PATH[] = "/.crosspoint/koreader.bin";
constexpr char BACKUP_PATH[] = "/.crosspoint/koreader.bin.bak";
constexpr uint8_t LEGACY_FILE_VERSION = 1;
constexpr uint8_t LEGACY_KEY[] = {0x4B, 0x4F, 0x52, 0x65, 0x61, 0x64, 0x65, 0x72};

template <typename T>
void appendPod(std::string& bytes, const T value) {
  bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void appendLegacyString(std::string& bytes, std::string value, const bool obfuscated = false) {
  const auto length = static_cast<uint32_t>(value.size());
  appendPod(bytes, length);
  if (obfuscated) {
    for (size_t i = 0; i < value.size(); ++i) value[i] ^= LEGACY_KEY[i % sizeof(LEGACY_KEY)];
  }
  bytes += value;
}

std::string legacyBinaryFixture() {
  std::string bytes;
  appendPod(bytes, LEGACY_FILE_VERSION);
  appendLegacyString(bytes, "legacy-reader");
  appendLegacyString(bytes, "legacy-secret", true);
  appendLegacyString(bytes, "https://legacy-sync.example.test");
  appendPod(bytes, static_cast<uint8_t>(DocumentMatchMethod::BINARY));
  return bytes;
}

class KOReaderCredentialStoreTest : public testing::Test {
 protected:
  void SetUp() override { Storage.reset(); }
  KOReaderCredentialStore& store() { return KOReaderCredentialStore::getInstance(); }
};

TEST_F(KOReaderCredentialStoreTest, MigratesForkJsonAndPreservesWifiModeAndCredentials) {
  const std::string oldForkJson =
      R"({"username":"fork-reader","password_obf":"test-obfuscated:fork-secret",)"
      R"("serverUrl":"https://sync.example.test","matchMethod":1,"koWifiConnectionMode":2})";
  Storage.setFile(JSON_PATH, oldForkJson);

  ASSERT_TRUE(store().loadFromFile());
  EXPECT_EQ(store().getUsername(), "fork-reader");
  EXPECT_EQ(store().getPassword(), "fork-secret");
  EXPECT_EQ(store().getServerUrl(), "https://sync.example.test");
  EXPECT_EQ(store().getMatchMethod(), DocumentMatchMethod::BINARY);
  EXPECT_EQ(store().getWifiConnectionMode(), KOReaderWifiConnectionMode::AUTO_CONNECT_LAST);
  EXPECT_EQ(store().getSyncBehavior(), KOReaderSyncBehavior::ASK_EVERY_TIME);
  EXPECT_FALSE(store().getSendMetadata());
  ASSERT_EQ(Storage.getWriteCount(), 1U);

  JsonDocument saved;
  ASSERT_FALSE(deserializeJson(saved, Storage.readFile(JSON_PATH)));
  EXPECT_EQ(saved["cfgVersion"].as<uint8_t>(), 2);
  EXPECT_EQ(saved["koWifiConnectionMode"].as<uint8_t>(), 2);
  EXPECT_EQ(saved["syncBehavior"].as<uint8_t>(), static_cast<uint8_t>(KOReaderSyncBehavior::ASK_EVERY_TIME));
  EXPECT_FALSE(saved["sendMetadata"].as<bool>());
  EXPECT_EQ(saved["password_obf"].as<std::string>(), "test-obfuscated:fork-secret");
  EXPECT_TRUE(saved["password"].isNull());

  ASSERT_TRUE(store().loadFromFile());
  EXPECT_EQ(Storage.getWriteCount(), 1U);
  EXPECT_EQ(store().getWifiConnectionMode(), KOReaderWifiConnectionMode::AUTO_CONNECT_LAST);
  EXPECT_EQ(store().getPassword(), "fork-secret");
}

TEST_F(KOReaderCredentialStoreTest, MigratesValidLegacyBinaryAndBacksItUp) {
  const std::string binary = legacyBinaryFixture();
  Storage.setFile(BINARY_PATH, binary);

  ASSERT_TRUE(store().loadFromFile());
  EXPECT_EQ(store().getUsername(), "legacy-reader");
  EXPECT_EQ(store().getPassword(), "legacy-secret");
  EXPECT_EQ(store().getServerUrl(), "https://legacy-sync.example.test");
  EXPECT_EQ(store().getMatchMethod(), DocumentMatchMethod::BINARY);
  EXPECT_EQ(store().getWifiConnectionMode(), KOReaderWifiConnectionMode::SMART);
  EXPECT_FALSE(Storage.exists(BINARY_PATH));
  ASSERT_TRUE(Storage.exists(BACKUP_PATH));
  EXPECT_EQ(Storage.readFile(BACKUP_PATH), binary);
  EXPECT_TRUE(Storage.exists(JSON_PATH));
  EXPECT_EQ(Storage.getWriteCount(), 1U);
}

TEST_F(KOReaderCredentialStoreTest, RejectsTruncatedLegacyStringWithoutReplacingBinary) {
  std::string binary;
  appendPod(binary, LEGACY_FILE_VERSION);
  appendPod(binary, static_cast<uint32_t>(4));
  binary += "abc";
  Storage.setFile(BINARY_PATH, binary);

  EXPECT_FALSE(store().loadFromFile());
  EXPECT_TRUE(Storage.exists(BINARY_PATH));
  EXPECT_EQ(Storage.readFile(BINARY_PATH), binary);
  EXPECT_FALSE(Storage.exists(BACKUP_PATH));
  EXPECT_FALSE(Storage.exists(JSON_PATH));
  EXPECT_EQ(Storage.getWriteCount(), 0U);
}

TEST_F(KOReaderCredentialStoreTest, RejectsOversizedLegacyStringWithoutReplacingBinary) {
  std::string binary;
  appendPod(binary, LEGACY_FILE_VERSION);
  appendPod(binary, static_cast<uint32_t>(65));
  Storage.setFile(BINARY_PATH, binary);

  EXPECT_FALSE(store().loadFromFile());
  EXPECT_TRUE(Storage.exists(BINARY_PATH));
  EXPECT_EQ(Storage.readFile(BINARY_PATH), binary);
  EXPECT_FALSE(Storage.exists(BACKUP_PATH));
  EXPECT_FALSE(Storage.exists(JSON_PATH));
  EXPECT_EQ(Storage.getWriteCount(), 0U);
}
}  // namespace
