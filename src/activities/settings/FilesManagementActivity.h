#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

class FilesManagementActivity final : public UiListActivity {
 private:
  bool lockLongPressBack = false;
  bool confirmPressedInActivity = false;
  bool backPressedInActivity = false;

  std::string basepath = "/";
  std::vector<std::string> files;
  char fileNameBuffer[500]{};
  char rowNameBuffer[512]{};
  char rowExtensionBuffer[16]{};

  void loadFiles();
  size_t findEntry(const std::string& name) const;
  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);
  bool isProtectedPath(const std::string& path) const;
  bool deleteRecursively(const std::string& path);

  int listCount() const override { return static_cast<int>(files.size()) + 1; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowLongPress(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void drawChrome() override;

  void openActionMenu(const std::string& entry, bool isDirectory);
  void handleRename(const std::string& fullPath, const std::string& entry, bool isDirectory);
  void handleMove(const std::string& fullPath, const std::string& entry, bool isDirectory);
  void handleDelete(const std::string& fullPath, const std::string& entry, bool isDirectory);
  void createFolder();

 public:
  explicit FilesManagementActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   std::string initialPath = "/")
      : UiListActivity("FilesManagement", renderer, mappedInput, /*wantsTouchLongPress=*/true),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)) {}

  void onEnter() override;
  void onExit() override;
};
