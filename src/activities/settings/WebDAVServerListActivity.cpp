#include "WebDAVServerListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "MappedInputManager.h"
#include "WebDAVServerStore.h"
#include "WebDAVSettingsActivity.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "activities/browser/WebDAVBrowserActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

WebDAVServerListActivity::WebDAVServerListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   const bool pickerMode)
    : UiListActivity("WebDAVServerList", renderer, mappedInput), pickerMode(pickerMode) {}

int WebDAVServerListActivity::getItemCount() const {
  const int serverCount = static_cast<int>(WEBDAV_STORE.getCount());
  return serverCount + (pickerMode ? 0 : 1);
}

void WebDAVServerListActivity::onEnter() {
  RenderLock lock(*this);
  UiListActivity::onEnter();
  WEBDAV_STORE.loadFromFile();
  nav.selected = 0;
}

void WebDAVServerListActivity::onBackButton() {
  if (pickerMode) {
    activityManager.goHome();
  } else {
    finish();
  }
}

const char* WebDAVServerListActivity::headerTitle() const { return tr(STR_WEBDAV_SERVERS); }

void WebDAVServerListActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();
  handleSelection();
}

void WebDAVServerListActivity::handleSelection() {
  const int serverCount = static_cast<int>(WEBDAV_STORE.getCount());
  if (nav.selected < 0 || nav.selected >= getItemCount()) return;

  if (pickerMode) {
    const auto* server = WEBDAV_STORE.getServer(static_cast<size_t>(nav.selected));
    if (server) {
      auto browser = makeUniqueNoThrow<WebDAVBrowserActivity>(renderer, mappedInput, *server);
      if (!browser) {
        LOG_ERR("WDS", "OOM: WebDAV browser activity");
        return;
      }
      activityManager.replaceActivity(std::move(browser));
    }
    return;
  }

  const int selected = nav.selected.load();
  const int serverIndex = selected < serverCount ? selected : -1;
  auto editor = makeUniqueNoThrow<WebDAVSettingsActivity>(renderer, mappedInput, serverIndex);
  if (!editor) {
    LOG_ERR("WDS", "OOM: WebDAV settings activity");
    return;
  }
  startActivityForResult(std::move(editor), [this](const ActivityResult&) {
    closeRouting();
    RenderLock lock(*this);
    WEBDAV_STORE.loadFromFile();
    nav.selected = 0;
    requestUpdate();
  });
}

void WebDAVServerListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const auto& servers = WEBDAV_STORE.getServers();
  const int serverCount = static_cast<int>(servers.size());
  const int itemCount = getItemCount();
  if (itemCount == 0) {
    screen.centeredText(tr(STR_NO_WEBDAV_SERVERS), screen.theme().bodyText);
    return;
  }

  for (int i = 0; i < serverCount; ++i) {
    auto& item = rowItems_[i];
    item = {};
    item.label = servers[i].name.empty() ? servers[i].url.c_str() : servers[i].name.c_str();
    if (!servers[i].name.empty()) item.subtitle = servers[i].url.c_str();
    item.actionValue = static_cast<int16_t>(i);
  }
  if (!pickerMode) {
    auto& item = rowItems_[serverCount];
    item = {};
    item.label = tr(STR_ADD_SERVER);
    item.actionValue = static_cast<int16_t>(serverCount);
  }

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(itemCount);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // Physical buttons stay in UiListActivity.
  syncListViewport(screen, props);
  screen.list(props);
}
