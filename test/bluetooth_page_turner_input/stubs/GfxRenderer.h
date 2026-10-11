#pragma once

class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  Orientation getOrientation() const { return Portrait; }
  void tapToLogical(float normalizedX, float normalizedY, int& x, int& y) const {
    x = static_cast<int>(normalizedX * getScreenWidth());
    y = static_cast<int>(normalizedY * getScreenHeight());
  }
  int getScreenWidth() const { return 800; }
  int getScreenHeight() const { return 480; }
};
