#include "WebDAVSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "MappedInputManager.h"
#include "activities/RenderLock.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr int BASE_ITEMS = 4;
}  // namespace

WebDAVSettingsActivity::WebDAVSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const int serverIndex)
    : UiListActivity("WebDAVSettings", renderer, mappedInput), serverIndex(serverIndex) {
  static constexpr StrId fieldNames[BASE_ITEMS] = {StrId::STR_SERVER_NAME, StrId::STR_WEBDAV_SERVER_URL,
                                                   StrId::STR_USERNAME, StrId::STR_PASSWORD};
  for (int i = 0; i < BASE_ITEMS; ++i) {
    fieldRowItems[i].label = I18N.get(fieldNames[i]);
    fieldRowItems[i].actionValue = static_cast<int16_t>(i);
  }
  fieldRowItems[BASE_ITEMS].label = tr(STR_DELETE_SERVER);
  fieldRowItems[BASE_ITEMS].actionValue = static_cast<int16_t>(BASE_ITEMS);
}

int WebDAVSettingsActivity::getMenuItemCount() const { return isNewServer ? BASE_ITEMS : BASE_ITEMS + 1; }

void WebDAVSettingsActivity::onEnter() {
  RenderLock lock(*this);
  UiListActivity::onEnter();
  nav.selected = 0;
  isNewServer = serverIndex < 0;
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
}

void WebDAVSettingsActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();
  handleSelection();
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
  if (nav.selected == 0) {
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        RenderLock lock(*this);
        editServer.name = std::get<KeyboardResult>(result.data).text;
        saveServer();
        requestUpdate();
      }
    };
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SERVER_NAME),
                                                             editServer.name, 63, InputType::Text);
    if (!keyboard) {
      LOG_ERR("WDS", "OOM: WebDAV keyboard activity");
      return;
    }
    startActivityForResult(std::move(keyboard), handler);
  } else if (nav.selected == 1) {
    const std::string prefillUrl = editServer.url.empty() ? "https://" : editServer.url;
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        RenderLock lock(*this);
        const auto& text = std::get<KeyboardResult>(result.data).text;
        editServer.url = (text == "https://" || text == "http://") ? "" : text;
        saveServer();
        requestUpdate();
      }
    };
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEBDAV_SERVER_URL),
                                                             prefillUrl, 127, InputType::Url);
    if (!keyboard) {
      LOG_ERR("WDS", "OOM: WebDAV keyboard activity");
      return;
    }
    startActivityForResult(std::move(keyboard), handler);
  } else if (nav.selected == 2) {
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        RenderLock lock(*this);
        editServer.username = std::get<KeyboardResult>(result.data).text;
        saveServer();
        requestUpdate();
      }
    };
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_USERNAME),
                                                             editServer.username, 63, InputType::Text);
    if (!keyboard) {
      LOG_ERR("WDS", "OOM: WebDAV keyboard activity");
      return;
    }
    startActivityForResult(std::move(keyboard), handler);
  } else if (nav.selected == 3) {
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        RenderLock lock(*this);
        editServer.password = std::get<KeyboardResult>(result.data).text;
        saveServer();
        requestUpdate();
      }
    };
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_PASSWORD),
                                                             editServer.password, 63, InputType::Password);
    if (!keyboard) {
      LOG_ERR("WDS", "OOM: WebDAV keyboard activity");
      return;
    }
    startActivityForResult(std::move(keyboard), handler);
  } else if (nav.selected == BASE_ITEMS && !isNewServer) {
    if (!WEBDAV_STORE.removeServer(static_cast<size_t>(serverIndex))) {
      LOG_ERR("WDS", "Failed to remove WebDAV server at index %d", serverIndex);
      showSaveError = true;
      requestUpdate();
      return;
    }
    finish();
  }
}

void WebDAVSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fieldRowItems[0].value = editServer.name.empty() ? tr(STR_NOT_SET) : editServer.name.c_str();
  fieldRowItems[1].value = editServer.url.empty() ? tr(STR_NOT_SET) : editServer.url.c_str();
  fieldRowItems[2].value = editServer.username.empty() ? tr(STR_NOT_SET) : editServer.username.c_str();
  fieldRowItems[3].value = editServer.password.empty() ? tr(STR_NOT_SET) : "******";

  fui::ListProps props;
  props.items = fieldRowItems;
  props.count = static_cast<uint16_t>(getMenuItemCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

const char* WebDAVSettingsActivity::headerTitle() const {
  return isNewServer ? tr(STR_ADD_SERVER) : tr(STR_WEBDAV_SERVERS);
}

void WebDAVSettingsActivity::drawFooter() {
  UiListActivity::drawFooter();
  if (showSaveError) GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
}
