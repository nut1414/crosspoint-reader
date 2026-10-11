#include "KOReaderCredentialStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <MD5Builder.h>
#include <ObfuscationUtils.h>
#include <Serialization.h>

namespace {
// Default sync server URL. crosspoint-sync speaks the full KOSync protocol, so
// pointing at any other kosync server (e.g. https://sync.koreader.rocks:443)
// still works via the custom server URL setting.
constexpr char DEFAULT_SERVER_URL[] = "https://sync.crosspointreader.com";

// Default before config version 2. Configs saved without a version stamp and an
// empty serverUrl were implicitly syncing here — they get pinned on upgrade.
constexpr char LEGACY_DEFAULT_SERVER_URL[] = "https://sync.koreader.rocks:443";

constexpr char LEGACY_FILE_BIN[] = "/.crosspoint/koreader.bin";
constexpr char LEGACY_FILE_BAK[] = "/.crosspoint/koreader.bin.bak";
constexpr uint8_t LEGACY_FILE_VERSION = 1;
constexpr uint8_t LEGACY_OBFUSCATION_KEY[] = {0x4B, 0x4F, 0x52, 0x65, 0x61, 0x64, 0x65, 0x72};

// Bumped when a change to defaults would alter behavior for existing configs.
constexpr uint8_t CONFIG_VERSION = 2;

bool readLegacyString(HalFile& file, std::string& value, const size_t maxLength) {
  if (file.available() < static_cast<int>(sizeof(uint32_t))) return false;

  uint32_t length = 0;
  serialization::readPod(file, length);
  if (length > maxLength || file.available() < static_cast<int>(length)) return false;

  value.resize(length);
  return length == 0 || file.read(value.data(), length) == static_cast<int>(length);
}

void legacyDeobfuscate(std::string& value) {
  for (size_t i = 0; i < value.size(); i++) {
    value[i] ^= LEGACY_OBFUSCATION_KEY[i % sizeof(LEGACY_OBFUSCATION_KEY)];
  }
}
}  // namespace

void KOReaderCredentialStore::toJson(JsonDocument& doc) const {
  doc["cfgVersion"] = CONFIG_VERSION;
  doc["username"] = getUsername();
  doc["password_obf"] = obfuscation::obfuscateToBase64(getPassword());
  doc["serverUrl"] = getServerUrl();
  doc["matchMethod"] = static_cast<uint8_t>(getMatchMethod());
  doc["sendMetadata"] = getSendMetadata();
  doc["syncBehavior"] = static_cast<uint8_t>(getSyncBehavior());
  doc["koWifiConnectionMode"] = static_cast<uint8_t>(getWifiConnectionMode());
}

bool KOReaderCredentialStore::fromJson(JsonVariantConst doc) {
  std::string user = doc["username"] | "";

  bool needsResave = false;
  std::string pass = extractPassword(doc, needsResave);

  setCredentials(user, pass);
  setServerUrl(doc["serverUrl"] | "");

  // The default server changed in config v2 (sync.koreader.rocks -> crosspoint-sync).
  // A pre-v2 config with credentials and no explicit URL was actively syncing
  // against the old default — pin that URL so the upgrade doesn't switch servers
  // out from under the user. Fresh setups get the new default.
  const uint8_t cfgVersion = doc["cfgVersion"] | (uint8_t)1;
  if (cfgVersion < CONFIG_VERSION) {
    if (getServerUrl().empty() && hasCredentials()) {
      LOG_DBG("KRS", "Pre-v2 config used the old default server; pinning %s", LEGACY_DEFAULT_SERVER_URL);
      setServerUrl(LEGACY_DEFAULT_SERVER_URL);
    }
    needsResave = true;  // stamp cfgVersion so this migration runs once
  }

  uint8_t method = doc["matchMethod"] | (uint8_t)0;
  if (method <= static_cast<uint8_t>(DocumentMatchMethod::BINARY)) {
    setMatchMethod(static_cast<DocumentMatchMethod>(method));
  } else {
    LOG_DBG("KRS", "Invalid matchMethod %u in JSON, resetting to FILENAME", method);
    setMatchMethod(DocumentMatchMethod::FILENAME);
  }
  setSendMetadata(doc["sendMetadata"] | false);

  const JsonVariantConst behaviorValue = doc["syncBehavior"];
  const bool missingBehavior = behaviorValue.isNull();
  uint8_t behavior = behaviorValue | static_cast<uint8_t>(KOReaderSyncBehavior::ASK_EVERY_TIME);
  if (behavior <= static_cast<uint8_t>(KOReaderSyncBehavior::SMART)) {
    setSyncBehavior(static_cast<KOReaderSyncBehavior>(behavior));
    needsResave = needsResave || missingBehavior;
  } else {
    LOG_DBG("KRS", "Invalid syncBehavior %u in JSON, resetting to ASK_EVERY_TIME", behavior);
    setSyncBehavior(KOReaderSyncBehavior::ASK_EVERY_TIME);
    needsResave = true;
  }

  const JsonVariantConst wifiModeValue = doc["koWifiConnectionMode"];
  const bool missingWifiMode = wifiModeValue.isNull();
  const uint8_t wifiMode = wifiModeValue | static_cast<uint8_t>(KOReaderWifiConnectionMode::SMART);
  setWifiConnectionMode(normalizeKOReaderWifiConnectionMode(wifiMode));
  needsResave = needsResave || missingWifiMode || wifiMode >= static_cast<uint8_t>(KOReaderWifiConnectionMode::COUNT);

  if (needsResave) {
    LOG_DBG("KRS", "Resaving KOReader credentials to update format");
    requestResave();
  }

  return true;
}

bool KOReaderCredentialStore::loadFromFile() {
  if (PersistableStore<KOReaderCredentialStore>::loadFromFile()) return true;
  if (!Storage.exists(LEGACY_FILE_BIN)) {
    LOG_DBG("KRS", "No credentials file found");
    return false;
  }

  if (!loadFromBinaryFile()) return false;
  if (!saveToFile()) {
    LOG_ERR("KRS", "Failed to save KOReader credentials during binary migration");
    return false;
  }
  if (!Storage.rename(LEGACY_FILE_BIN, LEGACY_FILE_BAK)) {
    LOG_ERR("KRS", "Failed to back up migrated KOReader credentials");
  }
  LOG_DBG("KRS", "Migrated koreader.bin to koreader.json");
  return true;
}

bool KOReaderCredentialStore::loadFromBinaryFile() {
  HalFile file;
  if (!Storage.openFileForRead("KRS", LEGACY_FILE_BIN, file)) return false;
  if (file.available() < 1) return false;

  username.clear();
  password.clear();
  serverUrl.clear();
  sendMetadata = false;
  syncBehavior = KOReaderSyncBehavior::SMART;
  wifiConnectionMode = KOReaderWifiConnectionMode::SMART;

  uint8_t version = 0;
  serialization::readPod(file, version);
  if (version != LEGACY_FILE_VERSION) {
    LOG_DBG("KRS", "Unknown binary file version: %u", version);
    return false;
  }

  if (file.available() > 0 && !readLegacyString(file, username, 64)) return false;
  if (file.available() > 0) {
    if (!readLegacyString(file, password, 64)) return false;
    legacyDeobfuscate(password);
  }
  if (file.available() > 0 && !readLegacyString(file, serverUrl, 128)) return false;

  if (serverUrl.empty() && hasCredentials()) serverUrl = LEGACY_DEFAULT_SERVER_URL;

  matchMethod = DocumentMatchMethod::FILENAME;
  if (file.available() > 0) {
    uint8_t method = 0;
    serialization::readPod(file, method);
    if (method <= static_cast<uint8_t>(DocumentMatchMethod::BINARY)) {
      matchMethod = static_cast<DocumentMatchMethod>(method);
    }
  }
  wifiConnectionMode = KOReaderWifiConnectionMode::SMART;

  LOG_DBG("KRS", "Loaded KOReader credentials from binary for user: %s", username.c_str());
  return true;
}

void KOReaderCredentialStore::setCredentials(const std::string& user, const std::string& pass) {
  username = user;
  password = pass;
  LOG_DBG("KRS", "Set credentials for user: %s", user.c_str());
}

std::string KOReaderCredentialStore::getMd5Password() const {
  if (password.empty()) {
    return "";
  }

  // Calculate MD5 hash of password using ESP32's MD5Builder
  MD5Builder md5;
  md5.begin();
  md5.add(password.c_str());
  md5.calculate();

  return md5.toString().c_str();
}

bool KOReaderCredentialStore::hasCredentials() const { return !username.empty() && !password.empty(); }

void KOReaderCredentialStore::clearCredentials() {
  username.clear();
  password.clear();
  saveToFile();
  LOG_DBG("KRS", "Cleared KOReader credentials");
}

void KOReaderCredentialStore::setServerUrl(const std::string& url) {
  serverUrl = url;
  LOG_DBG("KRS", "Set server URL: %s", url.empty() ? "(default)" : url.c_str());
}

std::string KOReaderCredentialStore::getBaseUrl() const {
  std::string url;
  if (serverUrl.empty()) {
    url = DEFAULT_SERVER_URL;
  } else if (serverUrl.find("://") == std::string::npos) {
    // Normalize URL: add http:// if no protocol specified (local servers typically don't have SSL)
    url = "http://" + serverUrl;
  } else {
    url = serverUrl;
  }

  // Strip trailing slashes to avoid double-slash in API paths
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }

  return url;
}

bool KOReaderCredentialStore::usesCrossPointSyncServer() const { return getBaseUrl() == DEFAULT_SERVER_URL; }

void KOReaderCredentialStore::setMatchMethod(DocumentMatchMethod method) {
  matchMethod = method;
  LOG_DBG("KRS", "Set match method: %s", method == DocumentMatchMethod::FILENAME ? "Filename" : "Binary");
}

void KOReaderCredentialStore::setSendMetadata(bool enabled) {
  sendMetadata = enabled;
  LOG_DBG("KRS", "Set send metadata: %s", enabled ? "true" : "false");
}

void KOReaderCredentialStore::setSyncBehavior(KOReaderSyncBehavior behavior) {
  if (static_cast<uint8_t>(behavior) > static_cast<uint8_t>(KOReaderSyncBehavior::SMART)) {
    behavior = KOReaderSyncBehavior::ASK_EVERY_TIME;
  }
  syncBehavior = behavior;
  LOG_DBG("KRS", "Set sync behavior: %s", behavior == KOReaderSyncBehavior::SMART ? "Smart" : "Ask");
}

void KOReaderCredentialStore::setWifiConnectionMode(const KOReaderWifiConnectionMode mode) {
  wifiConnectionMode = normalizeKOReaderWifiConnectionMode(static_cast<uint8_t>(mode));
  LOG_DBG("KRS", "Set WiFi connection mode: %u", static_cast<unsigned>(wifiConnectionMode));
}
