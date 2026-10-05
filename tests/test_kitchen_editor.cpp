#include "KitchenEditor.h"
#include <cassert>
#include <iostream>
using namespace leap;
int main() {
  KitchenEditor editor;
  editor.start();
  auto key = [&](bool right, Key k) { return editor.input({right, k, false}); };
  assert(!key(false, Key::Right));
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  assert(!key(true, Key::Center)); // Unchanged confirmation must not rewrite flash.
  key(false, Key::Center); // Pick up without deleting.
  key(false, Key::Right);
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  key(false, Key::Center); // Cancel.
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  key(false, Key::Left); key(false, Key::Center); key(false, Key::Right);
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Floor, 1) == -1);
  assert(editor.state.at(KitchenLayer::Floor, 2) >= 0);
  key(true, Key::Up); assert(key(true, Key::Center));
  assert(editor.state.objects[0].variant == 1);
  // A counter object requires support; switching layers chooses a compatible catalog item.
  key(false, Key::Up);
  key(true, Key::Right); // Coffee fits the single worktop unit.
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Counter, 2) >= 0);
  key(false, Key::Right);
  assert(!key(true, Key::Center));
  // Delete is the last catalog entry on every layer.
  key(false, Key::Left);
  key(true, Key::Right); key(true, Key::Right); key(true, Key::Right);
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Counter, 2) == -1);
  uint8_t before[KitchenState::SaveSize], after[KitchenState::SaveSize];
  editor.state.encode(before);
  assert(!editor.input({true, Key::Center, true}));
  editor.state.encode(after);
  assert(std::equal(before, before + sizeof(before), after));
  Arduino_GFX gfx;
  editor.draw(gfx, 94, 0, false);
  assert(gfx.text.find("L OK halten") != std::string::npos);
  for (auto r : gfx.rects) {
    assert(r.x >= 86 && r.y >= 0 && r.w > 0 && r.h > 0);
    assert(r.x + r.w <= 428 && r.y + r.h <= 142);
  }
  // Exercise all procedural object renderers, including wide items at the right edge.
  for (int type = 0; type < int(KitchenType::Count); ++type) {
    editor.state = KitchenState{};
    auto o = kitchenObject(KitchenType(type), 10, 3);
    if (o.layer == KitchenLayer::Counter) {
      editor.state.place(kitchenObject(KitchenType::Cabinet, 10, 0));
      editor.state.place(kitchenObject(KitchenType::Cabinet, 11, 0));
    }
    assert(editor.state.place(o));
    gfx.rects.clear(); editor.draw(gfx, 94, 0, false);
    for (auto r : gfx.rects) assert(r.x + r.w <= 428 && r.y + r.h <= 142);
  }
  std::cout << "PASS: kitchen dual-pad editing, cancel, color, support, delete and drawing bounds\n";
}
