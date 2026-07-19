#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "BookLogStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class BookLogActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  bool longPressFired = false;
  bool showingUnavailableMessage = false;
  std::string unavailableTitle;

  std::vector<BookLogEntry> books;
  std::vector<uint8_t> unavailable;

  void loadBookLog();
  void promptRemoveBook(const std::string& path, const std::string& title);

 public:
  explicit BookLogActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BookLog", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
