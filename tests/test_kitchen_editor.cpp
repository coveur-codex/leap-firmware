#include "KitchenEditor.h"
#include <cassert>
#include <iostream>
using namespace leap;
int main() {
  KitchenEditor editor;
  editor.start();
  int currentMode = int(KitchenAction::Build);
  auto key = [&](bool right, Key k) { return editor.input({right, k, false}); };
  auto mode = [&](KitchenAction wanted) {
    while (currentMode != int(wanted)) {
      assert(!key(false, Key::Down));
      currentMode = (currentMode + 1) % int(KitchenAction::Count);
    }
  };
  assert(!key(true, Key::Right));
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  assert(!key(true, Key::Center)); // Unchanged confirmation must not rewrite flash.
  mode(KitchenAction::Move);
  key(true, Key::Center); key(true, Key::Right);
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  mode(KitchenAction::Build); // Cancels without changing the saved object.
  assert(editor.state.at(KitchenLayer::Floor, 1) >= 0);
  key(true, Key::Left); mode(KitchenAction::Move);
  key(true, Key::Center); key(true, Key::Right);
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Floor, 1) == -1);
  assert(editor.state.at(KitchenLayer::Floor, 2) >= 0);
  mode(KitchenAction::Color);
  key(false, Key::Right); assert(key(true, Key::Center));
  assert(editor.state.objects[0].variant == 1);
  assert(!key(true, Key::Center));
  mode(KitchenAction::Build);
  key(true, Key::Up); key(false, Key::Right); // Coffee on the worktop.
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Counter, 2) >= 0);
  key(true, Key::Right); assert(!key(true, Key::Center)); // Unsupported.
  key(true, Key::Left); mode(KitchenAction::Remove);
  assert(key(true, Key::Center));
  assert(editor.state.at(KitchenLayer::Counter, 2) == -1);
  uint8_t before[KitchenState::SaveSize], after[KitchenState::SaveSize];
  editor.state.encode(before);
  mode(KitchenAction::Scroll);
  for (int i = 0; i < 40; ++i) assert(!key(true, Key::Right));
  Arduino_GFX gfx;
  gfx.recordAll = true;
  editor.draw(gfx, 94, 0, false);
  assert(gfx.text.find("Sicht 13-24") != std::string::npos);
  mode(KitchenAction::Build);
  key(true, Key::Down); // Floor again.
  for (int i = 0; i < 40; ++i) key(true, Key::Right);
  gfx.text.clear(); editor.draw(gfx, 94, 0, false);
  assert(gfx.text.find("24/24") != std::string::npos);
  assert(key(true, Key::Center)); // Build in the new half.
  assert(editor.state.at(KitchenLayer::Floor, 23) >= 0);
  key(false, Key::Right); key(false, Key::Right); // Sink, width 2.
  editor.state.encode(before);
  assert(!key(true, Key::Center)); // No wrapping at the right edge.
  editor.state.encode(after);
  assert(std::equal(before, before + sizeof(before), after));
  mode(KitchenAction::Scroll);
  for (int i = 0; i < 40; ++i) key(true, Key::Left);
  gfx.text.clear(); editor.draw(gfx, 94, 0, false);
  assert(gfx.text.find("Sicht 1-12") != std::string::npos);
  editor.state.encode(after);
  assert(std::equal(before, before + sizeof(before), after));
  // The real Input sampler must deliver a distinct 2 s exit after the regular hold.
  KitchenEditor heldEditor;
  heldEditor.start();
  heldEditor.state.place(kitchenObject(KitchenType::Cabinet, 0, 1));
  heldEditor.state.encode(before);
  Input sampled;
  sampled.begin();
  assert(sampled.start());
  fakeNow = 0; fakeUntil = 2000;
  fakeDown = [](int pin, uint32_t now) { return pin == hw::LeftKeys[4] && now >= 10 && now < 2600; };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  InputEvent event;
  while (sampled.poll(event)) {
    assert(!heldEditor.input(event));
    assert(!heldEditor.wantsExit());
  }
  fakeUntil = 3000;
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  assert(sampled.poll(event) && event.longPress && event.heldMs == 2000);
  assert(!heldEditor.input(event));
  assert(heldEditor.wantsExit());
  heldEditor.state.encode(after);
  assert(std::equal(before, before + sizeof(before), after));
  assert(!sampled.poll(event));
  heldEditor.start(); assert(!heldEditor.wantsExit());
  assert(!heldEditor.input({true, Key::Center, true, 2000}));
  assert(!heldEditor.wantsExit()); // Holding the placement key must not leave.

  mode(KitchenAction::Menu);
  key(true, Key::Right); // Kueche leeren.
  editor.state.encode(before);
  assert(!key(true, Key::Center));
  gfx.text.clear(); editor.draw(gfx, 94, 0, false);
  assert(gfx.text.find("Kueche leeren?") != std::string::npos);
  assert(!key(true, Key::Center)); // Default Abbrechen.
  editor.state.encode(after); assert(std::equal(before, before + sizeof(before), after));
  key(true, Key::Center); key(true, Key::Down); key(false, Key::Center);
  assert(editor.state.count != 0); // Left centre cancels even on Leeren.
  key(true, Key::Center);
  assert(!editor.input({true, Key::Center, true, 2000}));
  assert(editor.state.count != 0);
  key(true, Key::Right); assert(key(true, Key::Center));
  assert(editor.state.count == 0);
  key(true, Key::Center); key(true, Key::Right);
  assert(!key(true, Key::Center)); // Empty reset does not write flash.
  key(true, Key::Left); assert(!key(true, Key::Center));
  assert(editor.wantsExit());

  // Every type/variant, at both viewport edges, including partly visible wide objects.
  for (int type = 0; type < int(KitchenType::Count); ++type) {
    for (int color = 0; color < KitchenState::Variants; ++color) {
      for (int pos : {0, 10, 11, 12, 21}) {
        editor.state = KitchenState{};
        editor.start();
        auto o = kitchenObject(KitchenType(type), pos, color);
        if (o.layer == KitchenLayer::Counter)
          for (int x = pos; x < pos + o.width; ++x)
            assert(editor.state.place(kitchenObject(KitchenType::Cabinet, x, 0)));
        assert(editor.state.place(o));
        key(false, Key::Up); // Scroll mode.
        for (int view = 0; view <= 12; ++view) {
          gfx.rects.clear(); editor.draw(gfx, 94, 0, false);
          for (auto r : gfx.rects) {
            assert(r.x >= 94 && r.y >= 0 && r.w > 0 && r.h > 0);
            assert(r.x + r.w <= 418 && r.y + r.h <= 142);
          }
          key(true, Key::Right);
        }
      }
    }
  }
  std::cout << "PASS: kitchen modes, scrolling, atomic moves, recolor, delete, exit/reset, all asset clipping bounds\n";
}
