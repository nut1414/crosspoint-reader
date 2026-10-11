#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

/**
 * Simple folder picker that browses the local SD card and shows only directories.
 * Returns the selected folder path via KeyboardResult in ActivityResult.
 */
class FolderPickerActivity final : public UiListActivity {
 public:
  explicit FolderPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string initialPath = "/")
      : UiListActivity("FolderPicker", renderer, mappedInput), currentPath(std::move(initialPath)) {}

  void onEnter() override;

 private:
  char folderNameBuffer[500]{};
  std::string currentPath;
  std::vector<std::string> folders;
  std::vector<freeink::ui::ListItem> rowItems;

  int listCount() const override { return static_cast<int>(folders.size()) + 1; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawFooter() override;
  void loadFolders();
  void rebuildRowItems();
  void selectCurrentFolder();
  size_t findEntry(const std::string& name) const;
};
