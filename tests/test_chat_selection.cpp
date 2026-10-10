#include "ChatSelection.h"
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <cstdio>
using namespace leap;
int main() {
  ChatSelection c;
  c.vertical(-1, 2); assert(c.icons && c.icon == 32);
  c.horizontal(-1); assert(c.icon == 47);
  c.horizontal(1); assert(c.icon == 32);
  c.vertical(1, 2); assert(!c.icons && c.text == 0);
  c.vertical(1, 2); assert(!c.icons && c.text == 1);
  c.vertical(1, 2); assert(c.icons && c.icon == 0);
  c.horizontal(5); c.vertical(1, 2); assert(c.icon == 21);
  c.vertical(1, 2); assert(c.icon == 37);
  c.vertical(1, 2); assert(!c.icons && c.text == 0);
  c.vertical(-1, 2); assert(c.icons && c.icon == 37);
  c.vertical(-1, 2); c.vertical(-1, 2); c.vertical(-1, 2);
  assert(!c.icons && c.text == 1);
  c.vertical(1, 0); assert(c.icons && c.icon == 5);
  c.vertical(-1, 0); assert(c.icons && c.icon == 37);
  c.vertical(1, 0); assert(c.icon == 5);
  c.text = 100; c.icons = false;
  c.vertical(-1, 1); assert(c.icons); // Template removal remains safe.
  std::set<std::string> ids, symbols, glyphs;
  for (const auto &icon : ChatIcons) {
    assert(ids.insert(icon.id).second && symbols.insert(icon.symbol).second);
    assert(std::strlen(icon.id) < 37 && std::strlen(icon.symbol) <= 64);
    assert(icon.label[0] && chatSymbolRows(icon.symbol) == icon.rows);
    bool visible = false;
    std::string bitmap;
    for (uint16_t row : icon.rows) {
      assert(row < 4096); visible |= row != 0;
      bitmap += std::to_string(row) + ',';
    }
    assert(visible && glyphs.insert(bitmap).second);
  }
  assert(chatSymbolRows("❤") && !chatSymbolRows("unknown"));
  puts("PASS: text/grid transitions, columns, empty/changed templates and 48 unique visible local glyphs");
}
