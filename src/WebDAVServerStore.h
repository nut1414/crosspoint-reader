#pragma once
#include <string>
#include <vector>

struct WebDAVServer {
  std::string name;
  std::string url;
  std::string username;
  std::string password;  // Plaintext in memory; obfuscated with hardware key on disk
};

class WebDAVServerStore;
namespace JsonSettingsIO {
bool saveWebDAV(const WebDAVServerStore& store, const char* path);
bool loadWebDAV(WebDAVServerStore& store, const char* json, bool* needsResave);
}  // namespace JsonSettingsIO

/**
 * Singleton class for storing WebDAV server configurations on the SD card.
 * Passwords are XOR-obfuscated with the device's unique hardware MAC address
 * and base64-encoded before writing to JSON.
 */
class WebDAVServerStore {
 private:
  static WebDAVServerStore instance;
  std::vector<WebDAVServer> servers;

  static constexpr size_t MAX_SERVERS = 8;

  WebDAVServerStore() = default;

  friend bool JsonSettingsIO::saveWebDAV(const WebDAVServerStore&, const char*);
  friend bool JsonSettingsIO::loadWebDAV(WebDAVServerStore&, const char*, bool*);

 public:
  WebDAVServerStore(const WebDAVServerStore&) = delete;
  WebDAVServerStore& operator=(const WebDAVServerStore&) = delete;

  static WebDAVServerStore& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();

  bool addServer(const WebDAVServer& server);
  bool updateServer(size_t index, const WebDAVServer& server);
  bool removeServer(size_t index);

  const std::vector<WebDAVServer>& getServers() const { return servers; }
  const WebDAVServer* getServer(size_t index) const;
  size_t getCount() const { return servers.size(); }
  bool hasServers() const { return !servers.empty(); }
};

#define WEBDAV_STORE WebDAVServerStore::getInstance()
