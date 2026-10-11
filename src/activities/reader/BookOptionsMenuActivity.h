#pragma once

#include <I18n.h>

#include <array>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

class BookOptionsMenuActivity final : public UiListActivity {
 public:
  enum class Action { SELECT_CHAPTER, BROWSE_BOOK_FOLDER };

  explicit BookOptionsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                   bool hasChapters);

 private:
  struct MenuItem {
    Action action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildMenuItems(bool hasChapters);
  int listCount() const override { return static_cast<int>(menuItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title.c_str(); }
  void drawChrome() override;
  void onBackButton() override;

  const std::vector<MenuItem> menuItems;
  std::string title;
  std::array<freeink::ui::ListItem, 2> menuRowItems{};
};
