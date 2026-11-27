#pragma once
#include "PondMultiplayer.h"
#include "Input.h"
#include <Arduino_GFX_Library.h>

namespace leap {
class PondPost {
  LocalPondRace solo;
  PondFrame remote;
  bool menu = true, onlineMode = false, chooseOnline = false;
  bool rollSent = false, stopSent = false;
  uint32_t receivedAt = 0, rollingAt = 0, brakeStart = 1;

  const PondFrame &state() const { return onlineMode ? remote : solo.frame; }
  PondPhase phase() const;
  void startSolo(uint32_t now);

public:
  void open();
  void close();
  void input(Key key, uint32_t now);
  void tick(uint32_t now);
  bool animating() const { return !menu && (phase() == PondPhase::Rolling || phase() == PondPhase::Settling); }
  void draw(Arduino_GFX &gfx, uint16_t accent, uint32_t now) const;
};
} // namespace leap
