#pragma once
#include "Kitchen.h"
#include "Input.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class KitchenEditor {
  int position = 0, choice = 0, variant = 0, moving = -1;
  KitchenLayer layer = KitchenLayer::Floor;
  static constexpr int RemoveChoice = int(KitchenType::Count);
  static constexpr int BackChoice = RemoveChoice + 1, ResetChoice = RemoveChoice + 2;
  static constexpr int ChoiceCount = RemoveChoice + 3;
  bool blocked = false, leaveRequested = false, resetConfirm = false, confirmClear = false;
  void browse(int direction);
public:
  KitchenState state;
  void start();
  bool input(const InputEvent &event);
  bool wantsExit() const {
    return leaveRequested;
  }
  void draw(Arduino_GFX &gfx, int left, int top, bool savePending);
};
} // namespace leap
