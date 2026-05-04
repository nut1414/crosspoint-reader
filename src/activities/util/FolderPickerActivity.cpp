#include "FolderPickerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

void sortFolderList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    return strcasecmp(str1.c_str(), str2.c_str()) < 0;
  });
}

}  // namespace

void FolderPickerActivity::loadFolders() {
  folders.clear();

  auto root = Storage.open(currentPath.c_str());
  if (!root || !root.isDirectory()) {
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      continue;
    }
    if (file.isDirectory()) {
      folders.emplace_back(name);
    }
  }

  sortFolderList(folders);
}

void FolderPickerActivity::onEnter() {
  Activity::onEnter();

  if (currentPath.empty()) {
    currentPath = "/";
  }

  selectorIndex = 0;
  loadFolders();
  requestUpdate();
}

void FolderPickerActivity::onExit() { Activity::onExit(); }

void FolderPickerActivity::selectCurrentFolder() {
  KeyboardResult result;
  result.text = currentPath;
  setResult(ActivityResult(result));
  finish();
}

void FolderPickerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == SELECT_CURRENT_INDEX) {
      selectCurrentFolder();
      return;
    }

    if (!folders.empty()) {
      // Navigate into the selected folder
      if (currentPath.back() != '/') {
        currentPath += "/";
      }
      currentPath += folders[selectorIndex - 1];  // -1 because first item is virtual
      selectorIndex = 0;
      loadFolders();
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (currentPath != "/") {
      const std::string oldPath = currentPath;
      size_t lastSlash = currentPath.find_last_of('/');
      if (lastSlash != std::string::npos) {
        if (lastSlash == 0) {
          currentPath = "/";
        } else {
          currentPath = currentPath.substr(0, lastSlash);
        }
      }
      loadFolders();

      // Try to restore selection to the folder we just came from
      size_t pos = oldPath.find_last_of('/');
      if (pos != std::string::npos && pos + 1 < oldPath.length()) {
        std::string dirName = oldPath.substr(pos + 1);
        selectorIndex = findEntry(dirName) + 1;  // +1 for virtual item
      } else {
        selectorIndex = 0;
      }
      requestUpdate();
    } else {
      ActivityResult res;
      res.isCancelled = true;
      setResult(std::move(res));
      finish();
    }
    return;
  }

  int listSize = static_cast<int>(folders.size() + 1);  // +1 for "[Select Current Folder]"
  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, 23);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, 23);
    requestUpdate();
  });
}

void FolderPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SELECT_DEST_FOLDER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int itemCount = static_cast<int>(folders.size()) + 1;

  if (itemCount == 1 && folders.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_FILES_FOUND));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount, static_cast<int>(selectorIndex),
        [this](int index) {
          if (index == 0) {
            return std::string("[Select Current Folder]");
          }
          return folders[index - 1];
        },
        nullptr,
        [this](int index) {
          if (index == 0) {
            return Folder;
          }
          return Folder;
        });
  }

  const auto labels = mappedInput.mapLabels(currentPath == "/" ? tr(STR_CANCEL) : tr(STR_BACK), tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

size_t FolderPickerActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < folders.size(); i++)
    if (folders[i] == name) return i;
  return 0;
}
