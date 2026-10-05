#pragma once
#include "Kitchen.h"
#include "Input.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class KitchenEditor {
  int position = 0, choice = 0, variant = 0, moving = -1;
  KitchenLayer layer = KitchenLayer::Floor;
  bool blocked = false;
  void browse(int direction);
public:
  KitchenState state;
  void start();
  bool input(const InputEvent &event);
  void draw(Arduino_GFX &gfx, int left, int top, bool savePending);
};
} // namespace leap
