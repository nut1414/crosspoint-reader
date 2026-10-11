#include "FilesManagementActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "../util/ConfirmationActivity.h"
#include "../util/FolderPickerActivity.h"
#include "../util/KeyboardEntryActivity.h"
#include "FileActionMenuActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

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

void formatFileName(const std::string& filename, char* buffer, const size_t bufferSize) {
  if (filename.empty()) {
    buffer[0] = '\0';
    return;
  }
  const bool isDirectory = filename.back() == '/';
  const size_t dot = isDirectory ? filename.size() - 1 : filename.rfind('.');
  const int length = static_cast<int>(dot == std::string::npos ? filename.size() : dot);
  const char* format = isDirectory && !UITheme::getInstance().getTheme().showsFileIcons() ? "[%.*s]" : "%.*s";
  snprintf(buffer, bufferSize, format, length, filename.c_str());
}

void formatFileExtension(const std::string& filename, char* buffer, const size_t bufferSize) {
  buffer[0] = '\0';
  if (filename.empty() || filename.back() == '/') return;
  if (const char* extension = strrchr(filename.c_str(), '.')) snprintf(buffer, bufferSize, "%s", extension);
}
}  // namespace

void FilesManagementActivity::loadFiles() {
  files.clear();
  files.reserve(16);

  auto root = Storage.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    return;
  }

  root.rewindDirectory();

  char* const name = fileNameBuffer;
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(fileNameBuffer));
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

void FilesManagementActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<FilesManagementActivity*>(ctx);
  if (index >= self->listCount()) return;

  if (index == 0) {
    item.label = tr(STR_NEW_FOLDER);
    item.icon = listIconFor(Folder);
  } else {
    const std::string& entry = self->files[index - 1];
    formatFileName(entry, self->rowNameBuffer, sizeof(self->rowNameBuffer));
    item.label = self->rowNameBuffer;
    formatFileExtension(entry, self->rowExtensionBuffer, sizeof(self->rowExtensionBuffer));
    if (self->rowExtensionBuffer[0] != '\0') item.value = self->rowExtensionBuffer;
    item.icon = listIconFor(UITheme::getFileIcon(entry));
  }
  item.actionValue = static_cast<int16_t>(index);
}

void FilesManagementActivity::onEnter() {
  UiListActivity::onEnter();

  lockLongPressBack = mappedInput.isPressed(MappedInputManager::Button::Back);
  confirmPressedInActivity = false;
  backPressedInActivity = false;

  {
    RenderLock lock(*this);
    auto root = Storage.open(basepath.c_str());
    if (!root) basepath = "/";
    loadFiles();
    nav.reset();
  }
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
  char* const name = fileNameBuffer;
  bool allDeleted = true;
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(fileNameBuffer));
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

void FilesManagementActivity::handleRename(const std::string& fullPath, const std::string& entry, bool isDirectory) {
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
      {
        RenderLock lock(*this);
        loadFiles();
        nav.follow(listCount());
      }
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
      {
        RenderLock lock(*this);
        loadFiles();
        nav.follow(listCount());
      }
      requestUpdate(true);
    }
  };

  std::string currentPath;
  {
    RenderLock lock(*this);
    currentPath = basepath;
  }
  startActivityForResult(std::make_unique<FolderPickerActivity>(renderer, mappedInput, currentPath), handler);
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
      {
        RenderLock lock(*this);
        loadFiles();
        if (nav.selected >= listCount()) nav.selected = listCount() - 1;
        nav.follow(listCount());
      }
      requestUpdate(true);
    }
  };

  std::string heading = std::string(tr(STR_DELETE)) + "? ";
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, entry), handler);
}

void FilesManagementActivity::createFolder() {
  auto handler = [this](const ActivityResult& res) {
    if (res.isCancelled || !std::holds_alternative<KeyboardResult>(res.data)) return;
    const auto& kb = std::get<KeyboardResult>(res.data);
    if (kb.text.empty()) return;

    char sanitized[256];
    FsHelpers::sanitizePathComponentForFat32(kb.text.c_str(), sanitized, sizeof(sanitized));
    if (sanitized[0] == '\0') return;

    std::string currentPath;
    {
      RenderLock lock(*this);
      currentPath = basepath;
    }
    if (currentPath.back() != '/') currentPath += "/";
    const std::string newPath = currentPath + sanitized;
    if (!Storage.mkdir(newPath.c_str())) {
      LOG_ERR("FilesMgmt", "Failed to create folder: %s", newPath.c_str());
    }
    {
      RenderLock lock(*this);
      loadFiles();
      nav.follow(listCount());
    }
    requestUpdate(true);
  };

  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_FOLDER_NAME)),
                         handler);
}

void FilesManagementActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index == 0) {
    createFolder();
    return;
  }

  std::string entry;
  bool isDirectory = false;
  bool enteredDirectory = false;
  {
    RenderLock lock(*this);
    const size_t fileIndex = static_cast<size_t>(index - 1);
    if (fileIndex >= files.size()) return;
    entry = files[fileIndex];
    isDirectory = entry.back() == '/';
    if (isDirectory) {
      if (basepath.back() != '/') basepath += "/";
      basepath += entry.substr(0, entry.length() - 1);
      loadFiles();
      nav.reset();
      enteredDirectory = true;
    }
  }

  if (enteredDirectory) {
    requestUpdate();
  } else {
    openActionMenu(entry, isDirectory);
  }
}

void FilesManagementActivity::onRowLongPress(const int index) {
  if (index <= 0 || index >= listCount()) return;
  app.clearTapFlash();
  std::string entry;
  bool isDirectory = false;
  {
    RenderLock lock(*this);
    const size_t fileIndex = static_cast<size_t>(index - 1);
    if (fileIndex >= files.size()) return;
    entry = files[fileIndex];
    isDirectory = entry.back() == '/';
  }
  openActionMenu(entry, isDirectory);
}

bool FilesManagementActivity::handleCustomInput() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    confirmPressedInActivity = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    backPressedInActivity = true;
  }

  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= GO_HOME_MS &&
      !lockLongPressBack) {
    bool wentHome = false;
    {
      RenderLock lock(*this);
      if (basepath != "/") {
        basepath = "/";
        loadFiles();
        nav.reset();
        lockLongPressBack = true;
        wentHome = true;
      }
    }
    if (wentHome) {
      requestUpdate();
      return true;
    }
  }

  if (lockLongPressBack && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    lockLongPressBack = false;
    backPressedInActivity = false;
    return true;
  }

  return false;
}

bool FilesManagementActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const bool pressedInActivity = confirmPressedInActivity;
    confirmPressedInActivity = false;
    if (!pressedInActivity) return true;

    const int selected = nav.selected;
    if (selected < 0 || selected >= listCount()) return true;
    const unsigned long heldTime = mappedInput.getHeldTime();
    if (selected == 0 && heldTime >= LONG_PRESS_MS) return true;
    if (selected > 0 && heldTime >= LONG_PRESS_MS) {
      std::string entry;
      bool isDirectory = false;
      {
        RenderLock lock(*this);
        const size_t fileIndex = static_cast<size_t>(selected - 1);
        if (fileIndex >= files.size()) return true;
        entry = files[fileIndex];
        isDirectory = entry.back() == '/';
      }
      app.clearTapFlash();
      openActionMenu(entry, isDirectory);
    } else {
      activateIndex(selected);
    }
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    const bool pressedInActivity = backPressedInActivity;
    backPressedInActivity = false;
    if (!pressedInActivity) return true;

    if (mappedInput.getHeldTime() < GO_HOME_MS) {
      std::string oldPath;
      bool atRoot = false;
      {
        RenderLock lock(*this);
        atRoot = basepath == "/";
        if (!atRoot) {
          oldPath = basepath;
          basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
          if (basepath.empty()) basepath = "/";
          loadFiles();

          const auto pos = oldPath.find_last_of('/');
          const std::string dirName = oldPath.substr(pos + 1) + "/";
          nav.selected = static_cast<int>(findEntry(dirName)) + 1;
          nav.top = 0;
          nav.follow(listCount());
        }
      }
      if (atRoot) {
        finish();
      } else {
        requestUpdate();
      }
    }
    return true;
  }

  return false;
}

void FilesManagementActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  std::string folderName = (basepath == "/") ? tr(STR_SD_CARD) : basepath.substr(basepath.rfind('/') + 1);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 folderName.c_str());
}

void FilesManagementActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const int pathLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const fui::Rect pathBand = screen.takeBottom(static_cast<int16_t>(pathLineHeight + metrics.verticalSpacing));
  screen.target().fill(fui::Rect{pathBand.x, pathBand.y, pathBand.width, 3}, fui::Paint::solid(fui::Color::Black));
  const int pathY =
      pathBand.y + metrics.verticalSpacing / 2 + (pathBand.height - metrics.verticalSpacing / 2 - pathLineHeight) / 2;
  const int pathMaxWidth = pathBand.width - metrics.contentSidePadding * 2;
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
  renderer.drawText(SMALL_FONT_ID, pathBand.x + metrics.contentSidePadding, pathY, pathDisplay);

  if (files.empty()) {
    const auto text = screen.theme().smallText;
    screen.target().text(screen.takeTop(screen.target().lineHeight(text.font), metrics.verticalSpacing),
                         tr(STR_NO_FILES_FOUND), text);
  }

  fui::ListProps props;
  props.rowProvider = &FilesManagementActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.valueInset = 8;
  fui::TextStyle label = screen.theme().smallText;
  label.maxLines = 2;
  props.labelText = label;
  syncListViewport(screen, props);
  screen.list(props);
}

size_t FilesManagementActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < files.size(); i++)
    if (files[i] == name) return i;
  return 0;
}
