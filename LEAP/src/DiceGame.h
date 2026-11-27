#pragma once
#include "Input.h"
#include <array>

class Arduino_GFX;

namespace leap {
class DiceGame {
  static constexpr int MaxDice = 5;
  enum class Screen { Types, Count, Play };
  struct Die {
    uint8_t face = 0;
    bool held = false;
    uint16_t brakeMs = 0;
  };

  Screen screen = Screen::Types;
  int type = 0, count = 1, selected = 0;
  std::array<Die, MaxDice> dice{};
  uint32_t rolledAt = 0, rollDuration = 0;
  bool hasResult = false;

  int diceCount() const;
  void play();
  void roll(uint32_t now);

public:
  bool input(const InputEvent &event, uint32_t now);
  void tick(uint32_t now);
  bool animating() const { return rollDuration != 0; }
  void draw(Arduino_GFX &gfx, int left, uint16_t accent, uint32_t now) const;
};
} // namespace leap
