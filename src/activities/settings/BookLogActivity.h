#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "BookLogStore.h"
#include "activities/UiListActivity.h"

class BookLogActivity final : public UiListActivity {
  bool showingUnavailableMessage = false;
  std::string unavailableTitle;

  std::vector<BookLogEntry> books;
  std::vector<uint8_t> unavailable;
  std::vector<std::string> rowSubtitles;
  std::vector<freeink::ui::ListItem> rowItems;

  void loadBookLog();
  void rebuildRowItems();
  void promptRemoveBook(const std::string& path, const std::string& title);

 public:
  explicit BookLogActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("BookLog", renderer, mappedInput, /*wantsTouchLongPress=*/true) {}

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  int listCount() const override { return static_cast<int>(books.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowLongPress(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  const char* headerTitle() const override;
  void drawFooter() override;
};
