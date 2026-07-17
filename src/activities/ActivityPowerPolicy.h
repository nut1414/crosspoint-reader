#pragma once

namespace ActivityPowerPolicy {

struct Decision {
  bool resetAutoSleepTimer;
  bool resetFullSpeedTimer;
};

constexpr Decision decide(bool userActivity, bool preventAutoSleep, bool preventPowerSaving) {
  return {
      userActivity || preventAutoSleep,
      userActivity || preventPowerSaving,
  };
}

}  // namespace ActivityPowerPolicy
