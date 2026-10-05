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
  assert(gfx.text.find("L OK 2s halten") != std::string::npos);
  for (auto r : gfx.rects) {
    assert(r.x >= 86 && r.y >= 0 && r.w > 0 && r.h > 0);
    assert(r.x + r.w <= 428 && r.y + r.h <= 142);
  }
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

  // Reset requires choosing Leeren explicitly; a repeated hold cannot confirm it.
  KitchenEditor actions;
  actions.start();
  actions.state.place(kitchenObject(KitchenType::Cabinet, 0, 3));
  actions.state.encode(before);
  auto action = [&](bool right, Key k) { return actions.input({right, k, false}); };
  action(true, Key::Left); // Kueche leeren is available on every layer.
  assert(!action(true, Key::Center));
  gfx.text.clear(); actions.draw(gfx, 94, 0, false);
  assert(gfx.text.find("Kueche leeren?") != std::string::npos);
  assert(!action(true, Key::Center)); // Default Abbrechen.
  actions.state.encode(after); assert(std::equal(before, before + sizeof(before), after));
  assert(!action(true, Key::Center)); // Reopen confirmation.
  assert(!actions.input({true, Key::Center, true, 2000}));
  assert(actions.state.count == 1);
  action(true, Key::Down); action(false, Key::Center); // Left centre cancels even on Leeren.
  assert(actions.state.count == 1);
  action(true, Key::Center); action(true, Key::Right);
  assert(action(true, Key::Center));
  assert(actions.state.count == 0 && actions.state.valid());
  assert(!actions.wantsExit());
  action(true, Key::Left); action(true, Key::Center); action(true, Key::Right);
  assert(!action(true, Key::Center)); // Already empty: no redundant flash write.
  action(true, Key::Left); action(true, Key::Left); // Reset -> Zurueck.
  assert(!action(true, Key::Center));
  assert(actions.wantsExit() && actions.state.count == 0);
  // During a pending move the explicit exit also retains all saved objects.
  actions.start();
  actions.state.place(kitchenObject(KitchenType::Cabinet, 0, 3));
  action(false, Key::Center); action(false, Key::Right);
  action(true, Key::Left); action(true, Key::Left); action(true, Key::Center);
  assert(actions.wantsExit() && actions.state.count == 1 && actions.state.objects[0].position == 0);

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
  std::cout << "PASS: kitchen dual-pad editing, cancel, color, support, delete, sampled 2 s exit, explicit exit, confirmed reset and drawing bounds\n";
}
