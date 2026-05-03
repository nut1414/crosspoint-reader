#include "WebDAVServerListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "WebDAVServerStore.h"
#include "WebDAVSettingsActivity.h"
#include "activities/ActivityManager.h"
#include "activities/browser/WebDAVBrowserActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

int WebDAVServerListActivity::getItemCount() const {
  int count = static_cast<int>(WEBDAV_STORE.getCount());
  if (!pickerMode) {
    count++;
  }
  return count;
}

void WebDAVServerListActivity::onEnter() {
  Activity::onEnter();

  WEBDAV_STORE.loadFromFile();
  selectedIndex = 0;
  requestUpdate();
}

void WebDAVServerListActivity::onExit() { Activity::onExit(); }

void WebDAVServerListActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (pickerMode) {
      activityManager.goHome();
    } else {
      finish();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const int itemCount = getItemCount();
  if (itemCount > 0) {
    buttonNavigator.onNext([this, itemCount] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
      requestUpdate();
    });

    buttonNavigator.onPrevious([this, itemCount] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
      requestUpdate();
    });
  }
}

void WebDAVServerListActivity::handleSelection() {
  const auto serverCount = static_cast<int>(WEBDAV_STORE.getCount());

  if (pickerMode) {
    if (selectedIndex < serverCount) {
      const auto* server = WEBDAV_STORE.getServer(static_cast<size_t>(selectedIndex));
      if (server) {
        activityManager.replaceActivity(std::make_unique<WebDAVBrowserActivity>(renderer, mappedInput, *server));
      }
    }
    return;
  }

  auto resultHandler = [this](const ActivityResult&) {
    WEBDAV_STORE.loadFromFile();
    selectedIndex = 0;
  };

  if (selectedIndex < serverCount) {
    startActivityForResult(std::make_unique<WebDAVSettingsActivity>(renderer, mappedInput, selectedIndex), resultHandler);
  } else {
    startActivityForResult(std::make_unique<WebDAVSettingsActivity>(renderer, mappedInput, -1), resultHandler);
  }
}

void WebDAVServerListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WEBDAV_SERVERS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int itemCount = getItemCount();

  if (itemCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_WEBDAV_SERVERS));
  } else {
    const auto& servers = WEBDAV_STORE.getServers();
    const auto serverCount = static_cast<int>(servers.size());

    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount, selectedIndex,
        [&servers, serverCount](int index) {
          if (index < serverCount) {
            const auto& server = servers[index];
            return server.name.empty() ? server.url : server.name;
          }
          return std::string(I18n::getInstance().get(StrId::STR_ADD_SERVER));
        },
        [&servers, serverCount](int index) {
          if (index < serverCount && !servers[index].name.empty()) {
            return servers[index].url;
          }
          return std::string("");
        });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
