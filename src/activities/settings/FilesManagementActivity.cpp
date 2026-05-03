#include "FilesManagementActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "FileActionMenuActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "../util/ConfirmationActivity.h"
#include "../util/FolderPickerActivity.h"
#include "../util/KeyboardEntryActivity.h"

namespace {
constexpr unsigned long GO_HOME_MS = 1000;
constexpr unsigned long LONG_PRESS_MS = 500;

void sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    bool isDir1 = str1.back() == '/';
    bool isDir2 = str2.back() == '/';
    if (isDir1 != isDir2) return isDir1;

    const char* s1 = str1.c_str();
    const char* s2 = str2.c_str();

    while (*s1 && *s2) {
      if (isdigit(*s1) && isdigit(*s2)) {
        const char* start1 = s1;
        const char* start2 = s2;
        while (*s1 == '0') s1++;
        while (*s2 == '0') s2++;

        int len1 = 0, len2 = 0;
        while (isdigit(s1[len1])) len1++;
        while (isdigit(s2[len2])) len2++;

        if (len1 != len2) return len1 < len2;

        for (int i = 0; i < len1; i++) {
          if (s1[i] != s2[i]) return s1[i] < s2[i];
        }

        s1 += len1;
        s2 += len2;
      } else {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);
        if (c1 != c2) return c1 < c2;
        s1++;
        s2++;
      }
    }

    return *s1 == '\0' && *s2 != '\0';
  });
}

std::string getFileName(std::string filename) {
  if (filename.back() == '/') {
    filename.pop_back();
    if (!UITheme::getInstance().getTheme().showsFileIcons()) {
      return "[" + filename + "]";
    }
    return filename;
  }
  const auto pos = filename.rfind('.');
  return filename.substr(0, pos);
}

std::string getFileExtension(std::string filename) {
  if (filename.back() == '/') {
    return "";
  }
  const auto pos = filename.rfind('.');
  return filename.substr(pos);
}
}  // namespace

void FilesManagementActivity::loadFiles() {
  files.clear();

  auto root = Storage.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if ((!SETTINGS.showHiddenFiles && name[0] == '.') || strcmp(name, "System Volume Information") == 0) {
      continue;
    }

    if (file.isDirectory()) {
      files.emplace_back(std::string(name) + "/");
    } else {
      files.emplace_back(name);
    }
  }
  sortFileList(files);
}

void FilesManagementActivity::onEnter() {
  Activity::onEnter();

  selectorIndex = 0;
  lockLongPressBack = mappedInput.isPressed(MappedInputManager::Button::Back);
  confirmPressedInActivity = false;
  backPressedInActivity = false;

  auto root = Storage.open(basepath.c_str());
  if (!root) {
    basepath = "/";
    loadFiles();
  } else {
    loadFiles();
  }

  requestUpdate();
}

void FilesManagementActivity::onExit() {
  Activity::onExit();
  files.clear();
}

bool FilesManagementActivity::isProtectedPath(const std::string& path) const {
  return path == "/" || path == "/.crosspoint" || path.find("/.crosspoint/") == 0;
}

bool FilesManagementActivity::deleteRecursively(const std::string& path) {
  auto dir = Storage.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    return Storage.remove(path.c_str());
  }

  dir.rewindDirectory();
  char name[500];
  bool allDeleted = true;
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    std::string childPath = path;
    if (childPath.back() != '/') childPath += "/";
    childPath += name;

    if (file.isDirectory()) {
      if (!deleteRecursively(childPath)) {
        allDeleted = false;
      }
    } else {
      if (!Storage.remove(childPath.c_str())) {
        allDeleted = false;
      }
    }
  }

  if (allDeleted) {
    if (!Storage.rmdir(path.c_str())) {
      allDeleted = false;
    }
  }

  return allDeleted;
}

void FilesManagementActivity::openActionMenu(const std::string& entry, bool isDirectory) {
  std::string cleanBasePath = basepath;
  if (cleanBasePath.back() != '/') cleanBasePath += "/";
  const std::string fullPath = cleanBasePath + entry;

  auto handler = [this, fullPath, entry, isDirectory](const ActivityResult& res) {
    if (res.isCancelled) return;
    if (!std::holds_alternative<MenuResult>(res.data)) return;
    const auto& menu = std::get<MenuResult>(res.data);

    switch (static_cast<FileActionMenuActivity::Action>(menu.action)) {
      case FileActionMenuActivity::Action::Rename:
        handleRename(fullPath, entry, isDirectory);
        break;
      case FileActionMenuActivity::Action::Move:
        handleMove(fullPath, entry, isDirectory);
        break;
      case FileActionMenuActivity::Action::Delete:
        handleDelete(fullPath, entry, isDirectory);
        break;
    }
  };

  startActivityForResult(std::make_unique<FileActionMenuActivity>(renderer, mappedInput, entry, isDirectory), handler);
}

void FilesManagementActivity::handleRename(const std::string& fullPath, const std::string& entry,
                                           bool isDirectory) {
  if (isProtectedPath(fullPath)) {
    LOG_ERR("FilesMgmt", "Cannot rename protected item: %s", fullPath.c_str());
    return;
  }

  std::string initialName = entry;
  if (isDirectory && !initialName.empty() && initialName.back() == '/') {
    initialName.pop_back();
  }

  auto handler = [this, fullPath](const ActivityResult& res) {
    if (res.isCancelled) return;
    if (!std::holds_alternative<KeyboardResult>(res.data)) return;
    const auto& kb = std::get<KeyboardResult>(res.data);
    if (kb.text.empty()) return;

    char sanitized[256];
    FsHelpers::sanitizePathComponentForFat32(kb.text.c_str(), sanitized, sizeof(sanitized));
    if (sanitized[0] == '\0') return;

    std::string parent = FsHelpers::extractFolderPath(fullPath);
    if (parent.back() != '/') parent += "/";
    std::string newPath = parent + sanitized;

    if (isProtectedPath(newPath)) {
      LOG_ERR("FilesMgmt", "Cannot rename to protected path: %s", newPath.c_str());
      return;
    }

    if (!Storage.rename(fullPath.c_str(), newPath.c_str())) {
      LOG_ERR("FilesMgmt", "Failed to rename %s to %s", fullPath.c_str(), newPath.c_str());
    } else {
      loadFiles();
      requestUpdate(true);
    }
  };

  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_NEW_NAME), initialName), handler);
}

void FilesManagementActivity::handleMove(const std::string& fullPath, const std::string& entry, bool isDirectory) {
  auto handler = [this, fullPath, entry, isDirectory](const ActivityResult& res) {
    (void)isDirectory;
    if (res.isCancelled) return;
    if (!std::holds_alternative<KeyboardResult>(res.data)) return;
    const auto& kb = std::get<KeyboardResult>(res.data);
    if (kb.text.empty()) return;

    std::string destDir = kb.text;
    if (destDir.back() != '/') destDir += "/";

    if (destDir == "/.crosspoint/" || destDir.find("/.crosspoint/") == 0) {
      LOG_ERR("FilesMgmt", "Cannot move into protected folder: %s", destDir.c_str());
      return;
    }

    std::string destPath = destDir + entry;
    if (!Storage.rename(fullPath.c_str(), destPath.c_str())) {
      LOG_ERR("FilesMgmt", "Failed to move %s to %s", fullPath.c_str(), destPath.c_str());
    } else {
      loadFiles();
      requestUpdate(true);
    }
  };

  startActivityForResult(std::make_unique<FolderPickerActivity>(renderer, mappedInput, basepath), handler);
}

void FilesManagementActivity::handleDelete(const std::string& fullPath, const std::string& entry, bool isDirectory) {
  if (fullPath == "/") {
    LOG_ERR("FilesMgmt", "Cannot delete root");
    return;
  }

  auto handler = [this, fullPath, isDirectory](const ActivityResult& res) {
    if (res.isCancelled) return;

    bool success = false;
    if (isDirectory) {
      success = deleteRecursively(fullPath);
      if (!success) {
        LOG_ERR("FilesMgmt", "Failed to delete folder: %s", fullPath.c_str());
      }
    } else {
      success = Storage.remove(fullPath.c_str());
      if (!success) {
        LOG_ERR("FilesMgmt", "Failed to delete file: %s", fullPath.c_str());
      }
    }

    if (success) {
      loadFiles();
      if (files.empty()) {
        selectorIndex = 0;
      } else if (selectorIndex >= files.size() + 1) {
        selectorIndex = files.size();
      }
      requestUpdate(true);
    }
  };

  std::string heading = std::string(tr(STR_DELETE)) + "? ";
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, entry), handler);
}

void FilesManagementActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    confirmPressedInActivity = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    backPressedInActivity = true;
  }

  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= GO_HOME_MS &&
      basepath != "/" && !lockLongPressBack) {
    basepath = "/";
    loadFiles();
    selectorIndex = 0;
    lockLongPressBack = true;
    requestUpdate();
    return;
  }

  if (lockLongPressBack && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    lockLongPressBack = false;
    return;
  }

  const int pathReserved = renderer.getLineHeight(SMALL_FONT_ID) + UITheme::getInstance().getMetrics().verticalSpacing;
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false, pathReserved);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!confirmPressedInActivity) return;
    if (selectorIndex == 0) {
      // [New Folder]
      if (mappedInput.getHeldTime() >= LONG_PRESS_MS) {
        return;
      }
      auto handler = [this](const ActivityResult& res) {
        if (res.isCancelled) return;
        if (!std::holds_alternative<KeyboardResult>(res.data)) return;
        const auto& kb = std::get<KeyboardResult>(res.data);
        if (kb.text.empty()) return;

        char sanitized[256];
        FsHelpers::sanitizePathComponentForFat32(kb.text.c_str(), sanitized, sizeof(sanitized));
        if (sanitized[0] == '\0') return;

        std::string cleanBase = basepath;
        if (cleanBase.back() != '/') cleanBase += "/";
        std::string newPath = cleanBase + sanitized;

        if (!Storage.mkdir(newPath.c_str())) {
          LOG_ERR("FilesMgmt", "Failed to create folder: %s", newPath.c_str());
        }
        loadFiles();
        requestUpdate(true);
      };
      startActivityForResult(
          std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_FOLDER_NAME)), handler);
      return;
    }

    const std::string& entry = files[selectorIndex - 1];
    bool isDirectory = (entry.back() == '/');

    if (mappedInput.getHeldTime() >= LONG_PRESS_MS) {
      openActionMenu(entry, isDirectory);
      return;
    }

    if (isDirectory) {
      if (basepath.back() != '/') basepath += "/";
      basepath += entry.substr(0, entry.length() - 1);
      loadFiles();
      selectorIndex = 0;
      requestUpdate();
    } else {
      openActionMenu(entry, isDirectory);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (!backPressedInActivity) return;

    if (mappedInput.getHeldTime() < GO_HOME_MS) {
      if (basepath != "/") {
        const std::string oldPath = basepath;
        basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
        if (basepath.empty()) basepath = "/";
        loadFiles();

        const auto pos = oldPath.find_last_of('/');
        const std::string dirName = oldPath.substr(pos + 1) + "/";
        selectorIndex = findEntry(dirName) + 1;

        requestUpdate();
      } else {
        finish();
      }
    }
  }

  int listSize = static_cast<int>(files.size()) + 1;
  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void FilesManagementActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  std::string folderName = (basepath == "/") ? tr(STR_SD_CARD) : basepath.substr(basepath.rfind('/') + 1);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, folderName.c_str());

  const int pathLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int pathReserved = pathLineHeight + metrics.verticalSpacing;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing - pathReserved;

  int listSize = static_cast<int>(files.size()) + 1;

  if (listSize == 1 && files.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_FILES_FOUND));
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, listSize, static_cast<int>(selectorIndex),
                 [this](int index) {
                   if (index == 0) return std::string(tr(STR_NEW_FOLDER));
                   return getFileName(files[index - 1]);
                 },
                 nullptr,
                 [this](int index) {
                   if (index == 0) return Folder;
                   return UITheme::getFileIcon(files[index - 1]);
                 },
                 [this](int index) -> std::string {
                   if (index == 0) return "";
                   return getFileExtension(files[index - 1]);
                 },
                 false);
  }

  // Full path display
  {
    const int pathY = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - pathLineHeight;
    const int separatorY = pathY - metrics.verticalSpacing / 2;
    renderer.drawLine(0, separatorY, pageWidth - 1, separatorY, 3, true);
    const int pathMaxWidth = pageWidth - metrics.contentSidePadding * 2;
    const char* pathStr = basepath.c_str();
    const char* pathDisplay = pathStr;
    char leftTruncBuf[256];
    if (renderer.getTextWidth(SMALL_FONT_ID, pathStr) > pathMaxWidth) {
      const char ellipsis[] = "\xe2\x80\xa6";
      const int ellipsisWidth = renderer.getTextWidth(SMALL_FONT_ID, ellipsis);
      const int available = pathMaxWidth - ellipsisWidth;
      const char* p = pathStr;
      while (*p) {
        if (renderer.getTextWidth(SMALL_FONT_ID, p) <= available) break;
        ++p;
        while (*p && (static_cast<unsigned char>(*p) & 0xC0) == 0x80) ++p;
      }
      snprintf(leftTruncBuf, sizeof(leftTruncBuf), "%s%s", ellipsis, p);
      pathDisplay = leftTruncBuf;
    }
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, pathY, pathDisplay);
  }

  // Help text
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

size_t FilesManagementActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < files.size(); i++)
    if (files[i] == name) return i;
  return 0;
}
