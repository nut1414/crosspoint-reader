#include <ArduinoJson.h>
#include <HalStorage.h>
#include <ObfuscationUtils.h>
#include <gtest/gtest.h>

#include <string>

#include "WebDAVServerStore.h"

namespace obfuscation {
namespace {
constexpr char TEST_PREFIX[] = "test-obfuscated:";

std::string decode(const char* encoded, bool* ok) {
  if (ok) *ok = false;
  if (encoded == nullptr || std::string(encoded).compare(0, sizeof(TEST_PREFIX) - 1, TEST_PREFIX) != 0) {
    return "";
  }
  if (ok) *ok = true;
  return std::string(encoded + sizeof(TEST_PREFIX) - 1);
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
constexpr char WEBDAV_FILE_PATH[] = "/.crosspoint/webdav.json";

class WebDAVServerStoreTest : public testing::Test {
 protected:
  void SetUp() override { Storage.reset(); }

  WebDAVServerStore& store() { return WebDAVServerStore::getInstance(); }
};

TEST_F(WebDAVServerStoreTest, MigratesPlaintextPasswordAndResavesObfuscatedShapeOnce) {
  Storage.setFile(
      WEBDAV_FILE_PATH,
      R"({"servers":[{"name":"Home","url":"https://example.test","username":"reader","password":"secret"}]})");

  ASSERT_TRUE(store().loadFromFile());
  ASSERT_EQ(store().getCount(), 1U);
  EXPECT_EQ(store().getServer(0)->password, "secret");
  EXPECT_EQ(Storage.getWriteCount(), 1U);

  JsonDocument saved;
  ASSERT_FALSE(deserializeJson(saved, Storage.getFile(WEBDAV_FILE_PATH)));
  JsonObjectConst savedServer = saved["servers"][0].as<JsonObjectConst>();
  EXPECT_EQ(savedServer["password_obf"].as<std::string>(), "test-obfuscated:secret");
  EXPECT_TRUE(savedServer["password"].isNull());

  EXPECT_TRUE(store().loadFromFile());
  EXPECT_EQ(Storage.getWriteCount(), 1U);
  EXPECT_EQ(store().getServer(0)->password, "secret");
}

TEST_F(WebDAVServerStoreTest, PrefersObfuscatedPasswordAndCapsServerListAtEight) {
  std::string json = R"({"servers":[)";
  for (size_t i = 0; i < 10; ++i) {
    if (i != 0) json += ',';
    json += "{\"name\":\"Server " + std::to_string(i) +
            "\",\"url\":\"https://example.test/\",\"username\":\"reader\","
            "\"password_obf\":\"test-obfuscated:stored\",\"password\":\"stale\"}";
  }
  json += "]}";
  Storage.setFile(WEBDAV_FILE_PATH, json);

  ASSERT_TRUE(store().loadFromFile());
  ASSERT_EQ(store().getCount(), 8U);
  EXPECT_EQ(store().getServer(0)->name, "Server 0");
  EXPECT_EQ(store().getServer(7)->name, "Server 7");
  EXPECT_EQ(store().getServer(0)->password, "stored");
  EXPECT_EQ(Storage.getWriteCount(), 0U);
}
}  // namespace
