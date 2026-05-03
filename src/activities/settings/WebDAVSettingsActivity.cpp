#include "WebDAVSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>

#include "MappedInputManager.h"
#include "WebDAVServerStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Editable fields: Name, URL, Username, Password.
// Existing servers also show a Delete option (BASE_ITEMS + 1).
constexpr int BASE_ITEMS = 4;
}  // namespace

int WebDAVSettingsActivity::getMenuItemCount() const {
  return isNewServer ? BASE_ITEMS : BASE_ITEMS + 1;  // +1 for Delete
}

void WebDAVSettingsActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  isNewServer = (serverIndex < 0);
  showSaveError = false;

  if (!isNewServer) {
    const auto* server = WEBDAV_STORE.getServer(static_cast<size_t>(serverIndex));
    if (server) {
      editServer = *server;
    } else {
      isNewServer = true;
      serverIndex = -1;
    }
  }

  requestUpdate();
}

void WebDAVSettingsActivity::onExit() { Activity::onExit(); }

void WebDAVSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const int menuItems = getMenuItemCount();
  buttonNavigator.onNext([this, menuItems] {
    selectedIndex = (selectedIndex + 1) % menuItems;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuItems] {
    selectedIndex = (selectedIndex + menuItems - 1) % menuItems;
    requestUpdate();
  });
}

bool WebDAVSettingsActivity::saveServer() {
  bool success = false;

  if (isNewServer) {
    success = WEBDAV_STORE.addServer(editServer);
    if (success) {
      isNewServer = false;
      serverIndex = static_cast<int>(WEBDAV_STORE.getCount()) - 1;
    } else {
      LOG_ERR("WDS", "Failed to add WebDAV server");
    }
  } else {
    success = WEBDAV_STORE.updateServer(static_cast<size_t>(serverIndex), editServer);
    if (!success) {
      LOG_ERR("WDS", "Failed to update WebDAV server at index %d", serverIndex);
    }
  }

  showSaveError = !success;
  if (showSaveError) {
    requestUpdate();
  }

  return success;
}

void WebDAVSettingsActivity::handleSelection() {
  if (selectedIndex == 0) {
    // Server Name
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.name = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SERVER_NAME),
                                                                   editServer.name, 63, InputType::Text),
                           handler);
  } else if (selectedIndex == 1) {
    // Server URL
    const std::string prefillUrl = editServer.url.empty() ? "https://" : editServer.url;
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.url = (kb.text == "https://" || kb.text == "http://") ? "" : kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEBDAV_SERVER_URL),
                                                                   prefillUrl, 127, InputType::Url),
                           handler);
  } else if (selectedIndex == 2) {
    // Username
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.username = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_USERNAME),
                                                                   editServer.username, 63, InputType::Text),
                           handler);
  } else if (selectedIndex == 3) {
    // Password
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.password = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_PASSWORD),
                                                                   editServer.password, 63, InputType::Password),
                           handler);
  } else if (selectedIndex == 4 && !isNewServer) {
    // Delete
    if (!WEBDAV_STORE.removeServer(static_cast<size_t>(serverIndex))) {
      LOG_ERR("WDS", "Failed to remove WebDAV server at index %d", serverIndex);
      showSaveError = true;
      requestUpdate();
      return;
    }
    finish();
  }
}

void WebDAVSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const char* header = isNewServer ? tr(STR_ADD_SERVER) : tr(STR_WEBDAV_SERVERS);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.tabBarHeight;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int menuItems = getMenuItemCount();

  const StrId fieldNames[] = {StrId::STR_SERVER_NAME, StrId::STR_WEBDAV_SERVER_URL, StrId::STR_USERNAME,
                              StrId::STR_PASSWORD};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, menuItems, static_cast<int>(selectedIndex),
      [this, &fieldNames](int index) {
        if (index < BASE_ITEMS) {
          return std::string(I18N.get(fieldNames[index]));
        }
        return std::string(tr(STR_DELETE_SERVER));
      },
      nullptr, nullptr,
      [this](int index) {
        if (index == 0) {
          return editServer.name.empty() ? std::string(tr(STR_NOT_SET)) : editServer.name;
        } else if (index == 1) {
          return editServer.url.empty() ? std::string(tr(STR_NOT_SET)) : editServer.url;
        } else if (index == 2) {
          return editServer.username.empty() ? std::string(tr(STR_NOT_SET)) : editServer.username;
        } else if (index == 3) {
          return editServer.password.empty() ? std::string(tr(STR_NOT_SET)) : std::string("******");
        }
        return std::string("");
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (showSaveError) {
    GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
  }

  renderer.displayBuffer();
}
