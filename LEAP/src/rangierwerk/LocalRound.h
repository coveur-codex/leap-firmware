#pragma once
#include "Yard.h"

namespace rangierwerk {
struct LocalRound {
  inline static constexpr Index Minutes[]{5, 10, 15, 20, 0};
  uint32_t remaining = 0, last = 0;
  bool unlimited = false, finished = false;

  void start(Index duration, uint32_t now) {
    remaining = uint32_t(Minutes[duration]) * 60000;
    unlimited = !remaining;
    last = now;
    finished = false;
  }

  void advance(Yard &yard, uint32_t now, bool paused) {
    const uint32_t ms = now - last;
    last = now;
    if (paused || finished)
      return;
    const uint32_t active = !unlimited && ms >= remaining ? remaining : ms;
    yard.tick(active);
    if (!unlimited) {
      remaining -= active;
      finished = remaining == 0;
    }
  }
};
} // namespace rangierwerk
