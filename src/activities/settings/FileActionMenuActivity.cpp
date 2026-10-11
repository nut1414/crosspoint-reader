#include "FileActionMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

FileActionMenuActivity::FileActionMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::string& itemName, bool isDirectory)
    : UiListActivity("FileActionMenu", renderer, mappedInput), itemName(itemName), isDirectory(isDirectory) {
  menuItems.reserve(3);
  menuItems.push_back({Action::Rename, StrId::STR_RENAME});
  menuItems.push_back({Action::Move, StrId::STR_MOVE});
  menuItems.push_back({Action::Delete, StrId::STR_DELETE});
}

void FileActionMenuActivity::onEnter() {
  RenderLock lock(*this);
  UiListActivity::onEnter();
  for (size_t i = 0; i < menuItems.size(); ++i) {
    rowItems[i].label = I18N.get(menuItems[i].labelId);
    rowItems[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* FileActionMenuActivity::headerTitle() const { return itemName.c_str(); }

void FileActionMenuActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  nav.selected = index;
  app.clearTapFlash();
  setResult(MenuResult{static_cast<int>(menuItems[static_cast<size_t>(index)].action)});
  finish();
}

void FileActionMenuActivity::onBackButton() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void FileActionMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems;
  props.count = static_cast<uint16_t>(menuItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // buttons use UiListActivity navigation
  syncListViewport(screen, props);
  screen.list(props);
}
