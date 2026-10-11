#include "FolderPickerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {

void sortFolderList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    return strcasecmp(str1.c_str(), str2.c_str()) < 0;
  });
}

}  // namespace

void FolderPickerActivity::loadFolders() {
  folders.clear();
  folders.reserve(16);

  auto root = Storage.open(currentPath.c_str());
  if (!root || !root.isDirectory()) {
    rebuildRowItems();
    return;
  }

  root.rewindDirectory();

  char* const name = folderNameBuffer;
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(folderNameBuffer));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      continue;
    }
    if (file.isDirectory()) {
      folders.emplace_back(name);
    }
  }

  sortFolderList(folders);
  rebuildRowItems();
}

void FolderPickerActivity::rebuildRowItems() {
  rowItems.clear();
  rowItems.reserve(folders.size() + 1);

  fui::ListItem selectCurrent;
  selectCurrent.label = tr(STR_SELECT_CURRENT_FOLDER);
  if (folders.empty()) selectCurrent.subtitle = tr(STR_NO_FILES_FOUND);
  selectCurrent.icon = listIconFor(UIIcon::Folder);
  selectCurrent.actionValue = 0;
  rowItems.push_back(selectCurrent);

  for (size_t i = 0; i < folders.size(); ++i) {
    fui::ListItem item;
    item.label = folders[i].c_str();
    item.icon = listIconFor(UIIcon::Folder);
    item.actionValue = static_cast<int16_t>(i + 1);
    rowItems.push_back(item);
  }
}

void FolderPickerActivity::onEnter() {
  RenderLock lock(*this);
  UiListActivity::onEnter();

  if (currentPath.empty()) {
    currentPath = "/";
  }

  nav.reset();
  loadFolders();
  requestUpdate();
}

void FolderPickerActivity::selectCurrentFolder() {
  KeyboardResult result;
  result.text = currentPath;
  setResult(ActivityResult(result));
  finish();
}

const char* FolderPickerActivity::headerTitle() const { return tr(STR_SELECT_DEST_FOLDER); }

void FolderPickerActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(currentPath == "/" ? tr(STR_CANCEL) : tr(STR_BACK), tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void FolderPickerActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  nav.selected = index;
  app.clearTapFlash();

  if (index == 0) {
    selectCurrentFolder();
    return;
  }

  RenderLock lock(*this);
  closeRouting();
  if (currentPath.back() != '/') currentPath += "/";
  currentPath += folders[static_cast<size_t>(index - 1)];
  nav.reset();
  loadFolders();
  requestUpdate();
}

void FolderPickerActivity::onBackButton() {
  if (currentPath == "/") {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  RenderLock lock(*this);
  closeRouting();
  const std::string oldPath = currentPath;
  const size_t lastSlash = currentPath.find_last_of('/');
  if (lastSlash != std::string::npos) {
    currentPath = lastSlash == 0 ? "/" : currentPath.substr(0, lastSlash);
  }
  loadFolders();

  const size_t pos = oldPath.find_last_of('/');
  if (pos != std::string::npos && pos + 1 < oldPath.length()) {
    nav.selected = static_cast<int>(findEntry(oldPath.substr(pos + 1)) + 1);
  } else {
    nav.selected = 0;
  }
  nav.follow(listCount());
  requestUpdate();
}

void FolderPickerActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // buttons use UiListActivity navigation
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 1;
  syncListViewport(screen, props);
  screen.list(props);
}

size_t FolderPickerActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < folders.size(); i++)
    if (folders[i] == name) return i;
  return 0;
}
