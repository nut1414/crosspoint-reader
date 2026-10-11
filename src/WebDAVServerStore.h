#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct WebDAVServer {
  std::string name;
  std::string url;
  std::string username;
  std::string password;  // Plaintext in memory; obfuscated with hardware key on disk
};

/**
 * Singleton class for storing WebDAV server configurations on the SD card.
 * Passwords are XOR-obfuscated with the device's unique hardware MAC address
 * and base64-encoded before writing to JSON.
 */
class WebDAVServerStore : public PersistableStore<WebDAVServerStore> {
 private:
  std::vector<WebDAVServer> servers;

  static constexpr size_t MAX_SERVERS = 8;

  WebDAVServerStore() = default;

  friend class PersistableStore<WebDAVServerStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/webdav.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool addServer(const WebDAVServer& server);
  bool updateServer(size_t index, const WebDAVServer& server);
  bool removeServer(size_t index);

  const std::vector<WebDAVServer>& getServers() const { return servers; }
  const WebDAVServer* getServer(size_t index) const;
  size_t getCount() const { return servers.size(); }
  bool hasServers() const { return !servers.empty(); }
};

#define WEBDAV_STORE WebDAVServerStore::getInstance()
