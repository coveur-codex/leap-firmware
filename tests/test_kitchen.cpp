#include "Kitchen.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace leap;
int main() {
  KitchenState k;
  assert(k.valid() && k.count == 0);
  auto obj = [](KitchenType t, int p, int v = 0) { return kitchenObject(t, p, v); };
  assert(!k.place(obj(KitchenType::Coffee, 0)));
  assert(k.place(obj(KitchenType::Cabinet, 0)));
  assert(k.place(obj(KitchenType::Drawers, 1, 2)));
  assert(k.place(obj(KitchenType::Microwave, 0)));
  assert(k.place(obj(KitchenType::WallCabinet, 0)));
  uint8_t before[KitchenState::SaveSize], after[KitchenState::SaveSize];
  k.encode(before);
  assert(!k.place(obj(KitchenType::Fridge, 0)));
  assert(!k.place(obj(KitchenType::Sink, 0)));
  assert(!k.place(obj(KitchenType::Stove, 11)));
  assert(!k.place(obj(KitchenType::Cabinet, 5), k.at(KitchenLayer::Floor, 0)));
  k.encode(after); assert(memcmp(before, after, sizeof(before)) == 0);
  assert(k.place(obj(KitchenType::Toaster, 0, 3))); // Replace the whole microwave.
  assert(k.at(KitchenLayer::Counter, 1) == -1);
  int moving = k.at(KitchenLayer::Counter, 0);
  assert(k.place(obj(KitchenType::Toaster, 1, 3), moving, false));
  k.encode(after); assert(memcmp(before, after, sizeof(before)) != 0);
  assert(k.at(KitchenLayer::Counter, 0) == moving); // Preview did not mutate.
  assert(k.place(obj(KitchenType::Toaster, 1, 3), moving));
  assert(k.remove(KitchenLayer::Floor, 1));
  assert(k.at(KitchenLayer::Counter, 1) == -1 && k.valid());
  assert(k.place(obj(KitchenType::Fridge, 9)));
  assert(!k.place(obj(KitchenType::Hood, 8)));
  assert(!k.place(obj(KitchenType::Plant, 10)));
  assert(k.place(obj(KitchenType::WideWallCabinet, 4)));
  assert(k.place(obj(KitchenType::WallCabinet, 6)));
  assert(!k.place(obj(KitchenType::WideWallCabinet, 5))); // No accidental deletion of neighbour.
  assert(k.remove(KitchenLayer::Upper, 5)); // Interior of wide object.
  k.encode(before);
  KitchenState restored;
  assert(restored.decode(before, sizeof(before)));
  restored.encode(after); assert(memcmp(before, after, sizeof(before)) == 0);
  for (size_t i = 0; i < sizeof(before); ++i) {
    before[i] ^= 1;
    assert(!restored.decode(before, sizeof(before)));
    before[i] ^= 1;
  }
  assert(!restored.decode(before, sizeof(before) - 1));
  // Malformed logical objects must be rejected even with a correct checksum.
  before[4] = uint8_t(KitchenType::Count);
  uint32_t hash = KitchenState::checksum(before, sizeof(before) - 4);
  for (int i = 0; i < 4; ++i) before[sizeof(before) - 4 + i] = uint8_t(hash >> (i * 8));
  assert(!restored.decode(before, sizeof(before)));
  for (int type = 0; type < int(KitchenType::Count); ++type) {
    KitchenState scene;
    auto item = obj(KitchenType(type), 0, type % 4);
    if (item.layer == KitchenLayer::Counter) {
      assert(scene.place(obj(KitchenType::Cabinet, 0)));
      assert(scene.place(obj(KitchenType::Cabinet, 1)));
    }
    assert(scene.place(item));
    assert(scene.valid());
  }
  std::cout << "PASS: kitchen collisions, support, atomic edits, semantic save validation\n";
}
