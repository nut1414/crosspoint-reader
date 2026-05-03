#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class FilesManagementActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;
  bool lockLongPressBack = false;
  bool confirmPressedInActivity = false;
  bool backPressedInActivity = false;

  std::string basepath = "/";
  std::vector<std::string> files;

  void loadFiles();
  size_t findEntry(const std::string& name) const;
  bool isProtectedPath(const std::string& path) const;
  bool deleteRecursively(const std::string& path);

  void openActionMenu(const std::string& entry, bool isDirectory);
  void handleRename(const std::string& fullPath, const std::string& entry, bool isDirectory);
  void handleMove(const std::string& fullPath, const std::string& entry, bool isDirectory);
  void handleDelete(const std::string& fullPath, const std::string& entry, bool isDirectory);

 public:
  explicit FilesManagementActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   std::string initialPath = "/")
      : Activity("FilesManagement", renderer, mappedInput),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
