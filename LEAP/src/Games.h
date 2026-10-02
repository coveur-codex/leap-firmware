#pragma once
#include "Input.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class Games {
  String kind;
  uint32_t started = 0, last = 0;
  int score = 0, sequence[32]{}, length = 1, step = 0, show = 0, x = 0, y = 0;
  bool running = false, showing = false, won = false;

public:
  void start(const String &id);
  void input(Key key);
  void tick();
  void draw(Arduino_GFX &gfx, int left, int top);
  bool active() const {
    return running;
  }
};
} // namespace leap
