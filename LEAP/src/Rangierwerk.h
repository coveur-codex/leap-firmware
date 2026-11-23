#pragma once
#include "Input.h"
#include "RangierwerkMultiplayer.h"
#include "rangierwerk/Diagram.h"
#include "rangierwerk/LocalRound.h"
#include <Arduino_GFX_Library.h>

namespace leap {
class Rangierwerk {
  enum class Screen : uint8_t { Setup, Track, Couplings, Pause, Players };
  struct MotionFrame {
    uint32_t id = 0, movedAt = 0, bornAt = 0, linkAt = 0;
    rangierwerk::Index at = rangierwerk::None, from = rangierwerk::None;
    rangierwerk::Index a = rangierwerk::None, b = rangierwerk::None;
  };
  rangierwerk::Yard yard;
  rangierwerk::Diagram diagram;
  rangierwerk::LocalRound local;
  RangierFrame remote;
  std::array<MotionFrame, rangierwerk::MaxVehicles> animation{};
  Screen screen = Screen::Setup;
  bool onlineMode = false, waitRelease = false;
  int field = 0, mode = 0, level = 1, duration = 1, pause = 0;
  int turnout = 0, order = 0, cut = 0, held = 0;
  uint32_t card = 0, movedAt = 0, openedAt = 0, deliveryAt = 0, lastDelivered = 0;
  uint32_t lastSwitches = 0, switchedAt = 0, switched = 0;

  void startLocal(uint32_t now);
  void startOnline();
  bool playing() const;
  bool summary() const;
  void enterTrack();
  void discrete(RangierAction type, uint32_t a = 0, uint32_t b = 0);
  void animate(uint32_t now);
  rangierwerk::Consist consist() const;

  const auto &vehicles() const { return onlineMode ? remote.vehicles : yard.vehicles; }
  const auto &orders() const { return onlineMode ? remote.orders : yard.orders; }
  uint32_t delivered() const { return onlineMode ? remote.delivered : yard.delivered; }
  rangierwerk::Index self() const { return onlineMode ? remote.self : 0; }

public:
  void open();
  void close();
  bool input(const InputEvent &event); // True when the user exits the game.
  void directions(uint8_t mask);
  void tick(uint32_t now);
  void draw(Arduino_GFX &gfx, uint16_t accent) const;
  bool keepAwake() const { return screen != Screen::Setup && (!onlineMode || playing()); }
};
void drawRangierwerkIcon(Arduino_GFX &gfx, int x, int y, uint16_t accent);
} // namespace leap
