#include "WebDAVBrowserActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/FolderPickerActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/StringUtils.h"
#include "util/UrlUtils.h"

void WebDAVBrowserActivity::onEnter() {
  Activity::onEnter();

  state = BrowserState::CHECK_WIFI;
  items.clear();
  navigationHistory.clear();
  currentUrl = server.url;
  selectorIndex = 0;
  errorMessage.clear();
  statusMessage = tr(STR_CHECKING_WIFI);
  requestUpdate();

  checkAndConnectWifi();
}

void WebDAVBrowserActivity::onExit() {
  Activity::onExit();
  WiFi.mode(WIFI_OFF);
  items.clear();
  navigationHistory.clear();
}

void WebDAVBrowserActivity::loop() {
  if (state == BrowserState::WIFI_SELECTION || state == BrowserState::PICKING_DEST) {
    return;
  }

  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        state = BrowserState::LOADING;
        statusMessage = tr(STR_LOADING);
        requestUpdate();
        fetchDirectory(currentUrl);
      } else {
        launchWifiSelection();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }
    return;
  }

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (state == BrowserState::CHECK_WIFI) {
        onGoHome();
      } else {
        navigateBack();
      }
    }
    return;
  }

  if (state == BrowserState::DOWNLOADING) return;

  if (state == BrowserState::BROWSING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!items.empty()) {
        const auto& item = items[selectorIndex];
        item.isDirectory ? navigateToItem(item) : startDownload(item);
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }

    if (!items.empty()) {
      buttonNavigator.onNextRelease([this] {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, items.size());
        requestUpdate();
      });
      buttonNavigator.onPreviousRelease([this] {
        selectorIndex = ButtonNavigator::previousIndex(selectorIndex, items.size());
        requestUpdate();
      });
      buttonNavigator.onNextContinuous([this] {
        selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, items.size(), PAGE_ITEMS);
        requestUpdate();
      });
      buttonNavigator.onPreviousContinuous([this] {
        selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, items.size(), PAGE_ITEMS);
        requestUpdate();
      });
    }
  }
}

void WebDAVBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const char* headerTitle = server.name.empty() ? tr(STR_WEBDAV_BROWSER) : server.name.c_str();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, headerTitle, true, EpdFontFamily::BOLD);

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), pageWidth - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal > 0) {
      GUI.drawProgressBar(renderer, Rect{50, pageHeight / 2 + 20, pageWidth - 100, 20}, downloadProgress,
                          downloadTotal);
    }
    renderer.displayBuffer();
    return;
  }

  const char* confirmLabel =
      (!items.empty() && items[selectorIndex].isDirectory) ? tr(STR_OPEN) : tr(STR_DOWNLOAD);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (items.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_ENTRIES));
  } else {
    const auto pageStartIndex = selectorIndex / PAGE_ITEMS * PAGE_ITEMS;
    renderer.fillRect(0, 60 + (selectorIndex % PAGE_ITEMS) * 30 - 2, pageWidth - 1, 30);

    for (size_t i = pageStartIndex; i < items.size() && i < static_cast<size_t>(pageStartIndex + PAGE_ITEMS); i++) {
      const auto& item = items[i];
      std::string displayText = item.isDirectory ? "> " + item.name : item.name;
      if (!item.isDirectory && item.size > 0) {
        displayText += " (";
        if (item.size < 1024) {
          displayText += std::to_string(item.size) + " B";
        } else if (item.size < 1024 * 1024) {
          displayText += std::to_string(item.size / 1024) + " KB";
        } else {
          displayText += std::to_string(item.size / (1024 * 1024)) + " MB";
        }
        displayText += ")";
      }
      auto truncated = renderer.truncatedText(UI_10_FONT_ID, displayText.c_str(), pageWidth - 40);
      renderer.drawText(UI_10_FONT_ID, 20, 60 + (i % PAGE_ITEMS) * 30, truncated.c_str(),
                        i != static_cast<size_t>(selectorIndex));
    }
  }
  renderer.displayBuffer();
}

void WebDAVBrowserActivity::fetchDirectory(const std::string& url) {
  if (url.empty()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_NO_SERVER_URL);
    requestUpdate();
    return;
  }

  LOG_DBG("WEBDAV", "Fetching directory: %s", url.c_str());
  auto error = WebDAVClient::propfind(url, items, server.username, server.password);

  if (error != WebDAVError::OK) {
    state = BrowserState::ERROR;
    switch (error) {
      case WebDAVError::AUTH_ERROR:
        errorMessage = tr(STR_AUTH_FAILED);
        break;
      case WebDAVError::NETWORK_ERROR:
        errorMessage = tr(STR_WIFI_CONN_FAILED);
        break;
      default:
        errorMessage = tr(STR_FETCH_DIR_FAILED);
        break;
    }
    requestUpdate();
    return;
  }

  selectorIndex = 0;
  state = items.empty() ? BrowserState::ERROR : BrowserState::BROWSING;
  if (items.empty()) errorMessage = tr(STR_NO_ENTRIES);
  requestUpdate();
}

void WebDAVBrowserActivity::navigateToItem(const WebDAVItem& item) {
  navigationHistory.push_back(currentUrl);
  currentUrl = UrlUtils::buildUrl(currentUrl, item.href);
  // Ensure directory URLs end with / for consistent PROPFIND behavior
  if (!currentUrl.empty() && currentUrl.back() != '/') {
    currentUrl += '/';
  }

  state = BrowserState::LOADING;
  statusMessage = tr(STR_LOADING);
  items.clear();
  selectorIndex = 0;
  requestUpdate(true);
  fetchDirectory(currentUrl);
}

void WebDAVBrowserActivity::navigateBack() {
  if (navigationHistory.empty()) {
    onGoHome();
  } else {
    currentUrl = navigationHistory.back();
    navigationHistory.pop_back();
    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    items.clear();
    selectorIndex = 0;
    requestUpdate();
    fetchDirectory(currentUrl);
  }
}

void WebDAVBrowserActivity::startDownload(const WebDAVItem& item) {
  pendingDownloadUrl = UrlUtils::buildUrl(currentUrl, item.href);
  pendingDownloadName = item.name;

  state = BrowserState::PICKING_DEST;
  requestUpdate();

  auto picker = std::make_unique<FolderPickerActivity>(renderer, mappedInput, "/");
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      const auto& kb = std::get<KeyboardResult>(result.data);
      performDownload(kb.text);
    } else {
      state = BrowserState::BROWSING;
      requestUpdate();
    }
  });
}

void WebDAVBrowserActivity::performDownload(const std::string& destFolder) {
  state = BrowserState::DOWNLOADING;
  statusMessage = pendingDownloadName;
  downloadProgress = downloadTotal = 0;
  requestUpdate(true);

  std::string destPath = destFolder;
  if (!destPath.empty() && destPath.back() != '/') {
    destPath += '/';
  }
  destPath += StringUtils::sanitizeFilename(pendingDownloadName);

  LOG_DBG("WEBDAV", "Downloading: %s -> %s", pendingDownloadUrl.c_str(), destPath.c_str());

  auto error = WebDAVClient::downloadFile(
      pendingDownloadUrl, destPath,
      [this](const size_t downloaded, const size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
      },
      server.username, server.password);

  if (error == WebDAVError::OK) {
    if (FsHelpers::hasEpubExtension(destPath)) {
      Epub(destPath, "/.crosspoint").clearCache();
    }
    state = BrowserState::BROWSING;
  } else {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
  }
  requestUpdate();
}

void WebDAVBrowserActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate();
    fetchDirectory(currentUrl);
    return;
  }
  launchWifiSelection();
}

void WebDAVBrowserActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void WebDAVBrowserActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate(true);
    fetchDirectory(currentUrl);
  } else {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state = BrowserState::ERROR;
    errorMessage = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
  }
}
