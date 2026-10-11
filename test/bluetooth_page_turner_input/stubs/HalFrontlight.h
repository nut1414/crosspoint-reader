#pragma once

class TestFrontlight {
 public:
  bool present() const { return false; }
};

inline TestFrontlight Frontlight;
