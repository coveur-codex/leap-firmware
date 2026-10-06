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
  assert(!k.place(obj(KitchenType::Stove, 23)));
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
  KitchenState dining;
  assert(dining.place(obj(KitchenType::Table, 21, 2)));
  assert(dining.place(obj(KitchenType::FruitBowl, 22)));
  assert(dining.place(obj(KitchenType::Chair, 20)));
  assert(!dining.place(obj(KitchenType::Coffee, 20))); // A chair is no worktop.
  assert(!dining.place(obj(KitchenType::Chair, 22))); // Would orphan the bowl.
  assert(dining.remove(KitchenLayer::Floor, 23)); // Interior of a three-unit table.
  assert(dining.count == 1 && dining.objects[0].type == KitchenType::Chair);
  assert(!dining.place(obj(KitchenType::Table, 22)));
  KitchenState smallDining;
  assert(smallDining.place(obj(KitchenType::NarrowTable, 22, 1)));
  assert(smallDining.place(obj(KitchenType::Candle, 23, 3)));
  assert(smallDining.place(obj(KitchenType::ChairRight, 21, 2)));
  assert(!smallDining.place(obj(KitchenType::Mixer, 21))); // Chair cannot support half the mixer.
  assert(!smallDining.place(obj(KitchenType::Candle, 21)));
  assert(!smallDining.place(obj(KitchenType::NarrowTable, 23)));
  smallDining.encode(before); assert(restored.decode(before, sizeof(before)));
  assert(restored.objects[0] == obj(KitchenType::NarrowTable, 22, 1));
  assert(restored.objects[1] == obj(KitchenType::Candle, 23, 3));
  assert(restored.objects[2] == obj(KitchenType::ChairRight, 21, 2));
  assert(!smallDining.place(obj(KitchenType::ChairLeft, 22))); // Keep candle supported.
  assert(smallDining.remove(KitchenLayer::Floor, 23));
  assert(smallDining.count == 1 && smallDining.objects[0].type == KitchenType::ChairRight);
  assert(smallDining.place(obj(KitchenType::NarrowTable, 22)));
  assert(smallDining.place(obj(KitchenType::Mixer, 22)));
  assert(smallDining.place(obj(KitchenType::PlateShelf, 22)));
  KitchenState full;
  for (int pos = 0; pos < KitchenState::Columns; ++pos) {
    assert(full.place(obj(KitchenType::Cabinet, pos)));
    assert(full.place(obj(KitchenType::Coffee, pos)));
    assert(full.place(obj(KitchenType::WallCabinet, pos)));
  }
  assert(full.count == KitchenState::Capacity && full.valid());
  full.encode(before); assert(restored.decode(before, sizeof(before)));
  assert(restored.count == 72);
  uint8_t legacy[KitchenState::LegacySaveSize]{};
  legacy[0] = 'K'; legacy[1] = 'T'; legacy[2] = 1; legacy[3] = 1;
  legacy[4] = uint8_t(KitchenType::Stove); legacy[5] = 10;
  legacy[6] = uint8_t(KitchenLayer::Floor); legacy[7] = 2; legacy[8] = 3;
  auto legacyHash = [&]() {
    auto sum = KitchenState::checksum(legacy, sizeof(legacy) - 4);
    for (int i = 0; i < 4; ++i) legacy[sizeof(legacy) - 4 + i] = uint8_t(sum >> (i * 8));
  };
  legacyHash(); assert(restored.decode(legacy, sizeof(legacy)));
  assert(restored.count == 1 && restored.objects[0] == obj(KitchenType::Stove, 10, 3));
  restored.encode(before); assert(before[2] == 2);
  KitchenState migrated; assert(migrated.decode(before, sizeof(before)));
  legacy[5] = 11; legacyHash(); assert(!restored.decode(legacy, sizeof(legacy)));
  legacy[5] = 0; legacy[4] = uint8_t(KitchenType::Table); legacy[7] = 3;
  legacyHash(); assert(!restored.decode(legacy, sizeof(legacy)));
  assert(!restored.decode(nullptr, 0));
  std::cout << "PASS: kitchen collisions, support, atomic edits, semantic save validation\n";
}
