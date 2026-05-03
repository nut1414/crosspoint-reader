#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Activity showing the list of configured WebDAV servers.
 * Allows adding new servers and editing/deleting existing ones.
 * When pickerMode is true, selecting a server navigates to the WebDAV browser
 * instead of opening the editor (used from the home screen).
 */
class WebDAVServerListActivity final : public Activity {
 public:
  explicit WebDAVServerListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool pickerMode = false)
      : Activity("WebDAVServerList", renderer, mappedInput), pickerMode(pickerMode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool pickerMode = false;

  int getItemCount() const;
  void handleSelection();
};
