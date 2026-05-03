#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Simple folder picker that browses the local SD card and shows only directories.
 * Returns the selected folder path via KeyboardResult in ActivityResult.
 */
class FolderPickerActivity final : public Activity {
 public:
  explicit FolderPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                std::string initialPath = "/")
      : Activity("FolderPicker", renderer, mappedInput), currentPath(std::move(initialPath)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  std::string currentPath;
  std::vector<std::string> folders;

  static constexpr size_t SELECT_CURRENT_INDEX = 0;

  void loadFolders();
  void selectCurrentFolder();
  size_t findEntry(const std::string& name) const;
};
