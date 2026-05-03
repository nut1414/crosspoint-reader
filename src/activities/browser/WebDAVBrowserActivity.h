#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../Activity.h"
#include "WebDAVServerStore.h"
#include "network/WebDAVClient.h"
#include "util/ButtonNavigator.h"

/**
 * Activity for browsing and downloading files from a WebDAV server.
 * Supports navigation through directory hierarchy and downloading files.
 */
class WebDAVBrowserActivity final : public Activity {
 public:
  enum class BrowserState { CHECK_WIFI, WIFI_SELECTION, LOADING, BROWSING, PICKING_DEST, DOWNLOADING, ERROR };

  explicit WebDAVBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, WebDAVServer server)
      : Activity("WebDAVBrowser", renderer, mappedInput), buttonNavigator(), server(std::move(server)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  BrowserState state = BrowserState::LOADING;
  std::vector<WebDAVItem> items;
  std::vector<std::string> navigationHistory;
  std::string currentUrl;
  int selectorIndex = 0;
  std::string errorMessage;
  std::string statusMessage;
  bool truncated = false;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  std::string pendingDownloadUrl;
  std::string pendingDownloadName;

  WebDAVServer server;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void fetchDirectory(const std::string& url);
  void navigateToItem(const WebDAVItem& item);
  void navigateBack();
  void startDownload(const WebDAVItem& item);
  void performDownload(const std::string& destFolder);
  bool preventAutoSleep() override { return true; }

  static constexpr int PAGE_ITEMS = 23;
};
