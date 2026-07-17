#include "BookOptionsMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"

BookOptionsMenuActivity::BookOptionsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const std::string& title, const bool hasChapters)
    : Activity("BookOptionsMenu", renderer, mappedInput),
      menuItems(buildMenuItems(hasChapters)),
      title(title) {}

std::vector<BookOptionsMenuActivity::MenuItem> BookOptionsMenuActivity::buildMenuItems(const bool hasChapters) {
  std::vector<MenuItem> items;
  items.reserve(2);
  if (hasChapters) {
    items.push_back({Action::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  }
  items.push_back({Action::BROWSE_BOOK_FOLDER, StrId::STR_BROWSE_BOOK_FOLDER});
  return items;
}

void BookOptionsMenuActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void BookOptionsMenuActivity::onExit() { Activity::onExit(); }

void BookOptionsMenuActivity::loop() {
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(MenuResult{static_cast<int>(menuItems[selectedIndex].action)});
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
  }
}

void BookOptionsMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight},
                 title.c_str());

  const int contentTop = screen.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = screen.height - contentTop - metrics.verticalSpacing;
  GUI.drawList(renderer, Rect{screen.x, contentTop, screen.width, contentHeight}, menuItems.size(), selectedIndex,
               [this](int index) { return I18N.get(menuItems[index].labelId); }, nullptr, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
