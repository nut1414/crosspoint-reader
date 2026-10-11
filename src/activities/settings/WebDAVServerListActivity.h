#pragma once

#include <array>
#include <cstddef>

#include "activities/UiListActivity.h"

/**
 * Activity showing configured WebDAV servers.
 * Picker mode opens a server browser; settings mode also allows adding servers.
 */
class WebDAVServerListActivity final : public UiListActivity {
 public:
  explicit WebDAVServerListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool pickerMode = false);

  void onEnter() override;

 private:
  int listCount() const override { return getItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;

  static constexpr std::size_t MAX_ROW_ITEMS = 9;  // Eight servers plus Add Server.
  std::array<freeink::ui::ListItem, MAX_ROW_ITEMS> rowItems_{};
  bool pickerMode = false;

  int getItemCount() const;
  void handleSelection();
};
