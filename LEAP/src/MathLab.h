#pragma once
#include "Input.h"
#include "MathLabLogic.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class MathLab {
public:
  enum class Screen { Activities, Settings, Play };
  Screen screen = Screen::Activities;
  int activity = 0, variant = 0, difficulty = 0, setting = 0, seconds = 6;
  int a = 1, b = 1, target = 2, answer = 0, part = 0, tens = 0, ones = 0;
  int cursor = 0, selectedDie = 0, diceCount = 2, selectedPart = 0;
  bool feedback = false, correct = false, help = false, preview = false;
  uint32_t previewAt = 0;
  mathlab::Building original, built;
  mathlab::Histogram histogram;
  std::array<mathlab::Building, 3> views;
  int solution = 0, chosen = 0;
  unsigned found = 0;
  void start();
  // Returns true only when leaving the laboratory for the existing games menu.
  bool input(const InputEvent &e);
  void tick(uint32_t now);
  void draw(Arduino_GFX &gfx);
  void newTask();
  void check();
};
} // namespace leap
