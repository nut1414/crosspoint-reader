#pragma once

#include <I18n.h>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class FileActionMenuActivity final : public Activity {
 public:
  enum class Action { Rename, Move, Delete };

  explicit FileActionMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& itemName,
                                  bool isDirectory);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MenuItem {
    Action action;
    StrId labelId;
  };

  std::string itemName;
  bool isDirectory;
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;
  std::vector<MenuItem> menuItems;
};
