#pragma once
#include "Kitchen.h"
#include "Input.h"
#include <Arduino_GFX_Library.h>
namespace leap {
enum class KitchenAction : uint8_t { Scroll, Build, Move, Remove, Color, Menu, Count };
class KitchenEditor {
  int position = 0, choice = 0, variant = 0, moving = -1, viewport = 0, menuChoice = 0;
  KitchenLayer layer = KitchenLayer::Floor;
  KitchenAction action = KitchenAction::Build;
  bool blocked = false, leaveRequested = false, resetConfirm = false, confirmClear = false;
  void browse(int direction);
  void followCursor();
public:
  static constexpr int VisibleColumns = 12;
  KitchenState state;
  void start();
  bool input(const InputEvent &event);
  bool wantsExit() const { return leaveRequested; }
  void draw(Arduino_GFX &gfx, int left, int top, bool savePending);
};
} // namespace leap
