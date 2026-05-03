#include "WebDAVServerStore.h"

#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>

WebDAVServerStore WebDAVServerStore::instance;

namespace {
constexpr char WEBDAV_FILE_JSON[] = "/.crosspoint/webdav.json";
}  // namespace

bool WebDAVServerStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  return JsonSettingsIO::saveWebDAV(*this, WEBDAV_FILE_JSON);
}

bool WebDAVServerStore::loadFromFile() {
  if (Storage.exists(WEBDAV_FILE_JSON)) {
    String json = Storage.readFile(WEBDAV_FILE_JSON);
    if (!json.isEmpty()) {
      bool resave = false;
      bool result = JsonSettingsIO::loadWebDAV(*this, json.c_str(), &resave);
      if (result && resave) {
        LOG_DBG("WDS", "Resaving JSON with obfuscated passwords");
        saveToFile();
      }
      return result;
    }
  }
  return false;
}

bool WebDAVServerStore::addServer(const WebDAVServer& server) {
  if (servers.size() >= MAX_SERVERS) {
    LOG_DBG("WDS", "Cannot add more servers, limit of %zu reached", MAX_SERVERS);
    return false;
  }

  servers.push_back(server);
  LOG_DBG("WDS", "Added server: %s", server.name.c_str());
  return saveToFile();
}

bool WebDAVServerStore::updateServer(size_t index, const WebDAVServer& server) {
  if (index >= servers.size()) {
    return false;
  }

  servers[index] = server;
  LOG_DBG("WDS", "Updated server: %s", server.name.c_str());
  return saveToFile();
}

bool WebDAVServerStore::removeServer(size_t index) {
  if (index >= servers.size()) {
    return false;
  }

  LOG_DBG("WDS", "Removed server: %s", servers[index].name.c_str());
  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

const WebDAVServer* WebDAVServerStore::getServer(size_t index) const {
  if (index >= servers.size()) {
    return nullptr;
  }
  return &servers[index];
}
