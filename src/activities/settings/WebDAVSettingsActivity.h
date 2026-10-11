#pragma once

#include "WebDAVServerStore.h"
#include "activities/UiListActivity.h"

/** Edit one WebDAV server, saving each field as it is changed. */
class WebDAVSettingsActivity final : public UiListActivity {
 public:
  /** @param serverIndex Index into WebDAVServerStore, or -1 for a new server. */
  explicit WebDAVSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int serverIndex = -1);

  void onEnter() override;

 private:
  int listCount() const override { return getMenuItemCount(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawFooter() override;

  int serverIndex;
  WebDAVServer editServer;
  bool isNewServer = false;
  bool showSaveError = false;

  int getMenuItemCount() const;
  void handleSelection();
  bool saveServer();

  // Four editable fields plus Delete for an existing server; no heap allocation.
  static constexpr int MAX_MENU_ITEMS = 5;
  freeink::ui::ListItem fieldRowItems[MAX_MENU_ITEMS]{};
};
