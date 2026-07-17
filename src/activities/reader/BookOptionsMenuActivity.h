#pragma once

#include <I18n.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class BookOptionsMenuActivity final : public Activity {
 public:
  enum class Action { SELECT_CHAPTER, BROWSE_BOOK_FOLDER };

  explicit BookOptionsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                   bool hasChapters);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MenuItem {
    Action action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildMenuItems(bool hasChapters);

  const std::vector<MenuItem> menuItems;
  std::string title;
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;
};
