#include "WebDAVServerStore.h"

#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <utility>

void WebDAVServerStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["servers"].to<JsonArray>();
  for (const auto& server : servers) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = server.name;
    obj["url"] = server.url;
    obj["username"] = server.username;
    obj["password_obf"] = obfuscation::obfuscateToBase64(server.password);
  }
}

bool WebDAVServerStore::fromJson(JsonVariantConst doc) {
  servers.clear();
  JsonArrayConst arr = doc["servers"].as<JsonArrayConst>();
  servers.reserve(std::min(arr.size(), MAX_SERVERS));
  bool needsResave = false;

  for (JsonObjectConst obj : arr) {
    if (servers.size() >= MAX_SERVERS) break;
    WebDAVServer server;
    server.name = obj["name"] | "";
    server.url = obj["url"] | "";
    server.username = obj["username"] | "";
    server.password = extractPassword(obj, needsResave);
    servers.push_back(std::move(server));
  }

  LOG_DBG("WDS", "Loaded %zu WebDAV servers from file", servers.size());

  if (needsResave) {
    LOG_DBG("WDS", "Resaving JSON with obfuscated passwords");
    requestResave();
  }

  return true;
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
