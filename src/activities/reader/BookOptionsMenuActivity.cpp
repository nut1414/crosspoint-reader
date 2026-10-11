#include "BookOptionsMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BookOptionsMenuActivity::BookOptionsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const std::string& title, const bool hasChapters)
    : UiListActivity("BookOptionsMenu", renderer, mappedInput), menuItems(buildMenuItems(hasChapters)), title(title) {
  for (size_t i = 0; i < menuItems.size(); i++) {
    menuRowItems[i].label = I18N.get(menuItems[i].labelId);
    menuRowItems[i].actionValue = static_cast<int16_t>(i);
  }
}

std::vector<BookOptionsMenuActivity::MenuItem> BookOptionsMenuActivity::buildMenuItems(const bool hasChapters) {
  std::vector<MenuItem> items;
  items.reserve(2);
  if (hasChapters) {
    items.push_back({Action::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  }
  items.push_back({Action::BROWSE_BOOK_FOLDER, StrId::STR_BROWSE_BOOK_FOLDER});
  return items;
}

void BookOptionsMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = menuRowItems.data();
  props.count = static_cast<uint16_t>(menuItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}

void BookOptionsMenuActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  setResult(MenuResult{static_cast<int>(menuItems[index].action)});
  finish();
}

void BookOptionsMenuActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, Rect{safe.x, safe.y + metrics.topPadding, safe.width, metrics.headerHeight}, title.c_str());
}

void BookOptionsMenuActivity::onBackButton() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
