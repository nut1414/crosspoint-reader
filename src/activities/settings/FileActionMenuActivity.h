#pragma once

#include <I18n.h>

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

class FileActionMenuActivity final : public UiListActivity {
 public:
  enum class Action { Rename, Move, Delete };

  explicit FileActionMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& itemName,
                                  bool isDirectory);

  void onEnter() override;

 private:
  struct MenuItem {
    Action action;
    StrId labelId;
  };

  std::string itemName;
  bool isDirectory;
  std::vector<MenuItem> menuItems;
  freeink::ui::ListItem rowItems[3]{};

  int listCount() const override { return static_cast<int>(menuItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
};
