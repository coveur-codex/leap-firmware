#pragma once
#include "ChatSymbols.h"
#include <algorithm>
namespace leap {
// Up/down walks text templates and then the three icon rows. The column is
// remembered when leaving the grid; left/right wraps inside the current row.
struct ChatSelection {
  bool icons = false;
  int text = 0, icon = 0;
  void vertical(int delta, int textCount) {
    if (!delta) return;
    text = std::clamp(text, 0, std::max(0, textCount - 1));
    if (icons) {
      int row = icon / ChatIconColumns + delta;
      if (row < 0 || row >= ChatIconRows) {
        if (textCount) { icons = false; text = row < 0 ? textCount - 1 : 0; }
        else icon = ((row + ChatIconRows) % ChatIconRows) * ChatIconColumns + icon % ChatIconColumns;
      } else icon = row * ChatIconColumns + icon % ChatIconColumns;
    } else {
      int next = text + delta;
      if (!textCount || next < 0 || next >= textCount) {
        icons = true;
        icon = (delta < 0 ? ChatIconRows - 1 : 0) * ChatIconColumns + icon % ChatIconColumns;
      } else text = next;
    }
  }
  void horizontal(int delta) {
    if (icons) icon = (icon / ChatIconColumns) * ChatIconColumns +
        (icon % ChatIconColumns + delta + ChatIconColumns) % ChatIconColumns;
  }
};
} // namespace leap
