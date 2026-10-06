#pragma once
#include <cstddef>
#include <cstdint>
namespace leap {
enum class KitchenLayer : uint8_t { Floor, Counter, Upper };
// Stable semantic IDs are also the on-device format; append future types.
enum class KitchenType : uint8_t {
  Cabinet, Drawers, Sink, Stove, Dishwasher, Fridge, Pantry,
  WallCabinet, WideWallCabinet, Shelf, Hood, Microwave, Coffee, Toaster, Plant,
  Table, Chair, Kettle, KnifeBlock, FruitBowl, SpiceRack, Clock, Count
};
struct KitchenSpec {
  const char *name;
  KitchenLayer layer;
  uint8_t width;
};
inline constexpr KitchenSpec KitchenCatalog[] = {
    {"Unterschrank", KitchenLayer::Floor, 1}, {"Schubladen", KitchenLayer::Floor, 1},
    {"Spuele", KitchenLayer::Floor, 2}, {"Herd + Ofen", KitchenLayer::Floor, 2},
    {"Geschirrspueler", KitchenLayer::Floor, 1}, {"Kuehlschrank", KitchenLayer::Floor, 2},
    {"Hochschrank", KitchenLayer::Floor, 2}, {"Haengeschrank", KitchenLayer::Upper, 1},
    {"Breiter Schrank", KitchenLayer::Upper, 2}, {"Regal", KitchenLayer::Upper, 2},
    {"Abzugshaube", KitchenLayer::Upper, 2}, {"Mikrowelle", KitchenLayer::Counter, 2},
    {"Kaffeemaschine", KitchenLayer::Counter, 1}, {"Toaster", KitchenLayer::Counter, 1},
    {"Pflanze", KitchenLayer::Counter, 1}, {"Esstisch", KitchenLayer::Floor, 3},
    {"Stuhl", KitchenLayer::Floor, 1}, {"Wasserkocher", KitchenLayer::Counter, 1},
    {"Messerblock", KitchenLayer::Counter, 1}, {"Obstschale", KitchenLayer::Counter, 2},
    {"Gewuerzregal", KitchenLayer::Upper, 2}, {"Wanduhr", KitchenLayer::Upper, 1}};
struct KitchenObject {
  KitchenType type = KitchenType::Cabinet;
  uint8_t position = 0;
  KitchenLayer layer = KitchenLayer::Floor;
  uint8_t width = 1, variant = 0;
  bool operator==(const KitchenObject &other) const {
    return type == other.type && position == other.position && layer == other.layer &&
           width == other.width && variant == other.variant;
  }
};
inline KitchenObject kitchenObject(KitchenType type, int position, int variant) {
  auto spec = KitchenCatalog[int(type)];
  return {type, uint8_t(position), spec.layer, spec.width, uint8_t(variant)};
}
class KitchenState {
public:
  static constexpr int Columns = 24, Capacity = Columns * 3, Variants = 4;
  static constexpr size_t LegacySaveSize = 188;
  static constexpr size_t SaveSize = 4 + Capacity * 5 + 4;
  KitchenObject objects[Capacity]{};
  uint8_t count = 0;
  static bool tall(KitchenType type) {
    return type == KitchenType::Fridge || type == KitchenType::Pantry;
  }
  static bool supports(KitchenType type) {
    return type == KitchenType::Cabinet || type == KitchenType::Drawers ||
           type == KitchenType::Dishwasher || type == KitchenType::Table;
  }
  static bool overlaps(KitchenObject a, KitchenObject b) {
    return a.position < b.position + b.width && b.position < a.position + a.width;
  }
  static bool validObject(KitchenObject o) {
    if (int(o.type) >= int(KitchenType::Count) || o.variant >= Variants)
      return false;
    auto spec = KitchenCatalog[int(o.type)];
    return o.layer == spec.layer && o.width == spec.width && o.position + o.width <= Columns;
  }
  int at(KitchenLayer layer, int position) const {
    for (int i = 0; i < count; ++i)
      if (objects[i].layer == layer && position >= objects[i].position &&
          position < objects[i].position + objects[i].width)
        return i;
    return -1;
  }
  bool valid() const {
    if (count > Capacity)
      return false;
    for (int i = 0; i < count; ++i) {
      auto a = objects[i];
      if (!validObject(a))
        return false;
      for (int j = 0; j < i; ++j) {
        auto b = objects[j];
        if (overlaps(a, b) && (a.layer == b.layer || tall(a.type) || tall(b.type)))
          return false;
      }
      if (a.layer == KitchenLayer::Counter)
        for (int x = a.position; x < a.position + a.width; ++x) {
          int support = at(KitchenLayer::Floor, x);
          if (support < 0 || !supports(objects[support].type))
            return false;
        }
    }
    return true;
  }
  void eraseIndex(int index) {
    for (int i = index + 1; i < count; ++i)
      objects[i - 1] = objects[i];
    --count;
  }
  // Transactional edit: failed moves/replacements retain the entire old kitchen.
  bool place(KitchenObject object, int moving = -1, bool commit = true) {
    if (!validObject(object) || moving < -1 || moving >= count)
      return false;
    int replacing = at(object.layer, object.position);
    if (replacing >= 0 && (moving == -1 || moving == replacing) && objects[replacing] == object)
      return true;
    KitchenState next = *this;
    for (int i = count - 1; i >= 0; --i)
      if (i == moving || i == replacing)
        next.eraseIndex(i);
    if (next.count == Capacity)
      return false;
    next.objects[next.count++] = object;
    if (!next.valid())
      return false;
    if (commit)
      *this = next;
    return true;
  }
  bool remove(KitchenLayer layer, int position) {
    int index = at(layer, position);
    if (index < 0)
      return false;
    // Removing a worktop also removes decorations it supports.
    auto removed = objects[index];
    for (int i = count - 1; i >= 0; --i)
      if (i == index || (removed.layer == KitchenLayer::Floor &&
                        objects[i].layer == KitchenLayer::Counter && overlaps(removed, objects[i])))
        eraseIndex(i);
    return true;
  }
  static uint32_t checksum(const uint8_t *bytes, size_t n) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < n; ++i)
      hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
  }
  void encode(uint8_t (&bytes)[SaveSize]) const {
    for (auto &byte : bytes) byte = 0;
    bytes[0] = 'K'; bytes[1] = 'T'; bytes[2] = 2; bytes[3] = count;
    for (int i = 0; i < count; ++i) {
      auto o = objects[i];
      auto p = bytes + 4 + i * 5;
      p[0] = uint8_t(o.type); p[1] = o.position; p[2] = uint8_t(o.layer);
      p[3] = o.width; p[4] = o.variant;
    }
    uint32_t hash = checksum(bytes, SaveSize - 4);
    for (int i = 0; i < 4; ++i) bytes[SaveSize - 4 + i] = uint8_t(hash >> (i * 8));
  }
  bool decode(const uint8_t *bytes, size_t size) {
    if (!bytes || (size != SaveSize && size != LegacySaveSize)) return false;
    bool legacy = size == LegacySaveSize;
    if (bytes[0] != 'K' || bytes[1] != 'T' || bytes[2] != (legacy ? 1 : 2) ||
        bytes[3] > (legacy ? 36 : Capacity)) return false;
    uint32_t hash = 0;
    for (int i = 0; i < 4; ++i) hash |= uint32_t(bytes[size - 4 + i]) << (i * 8);
    if (hash != checksum(bytes, size - 4)) return false;
    KitchenState next;
    next.count = bytes[3];
    for (int i = 0; i < next.count; ++i) {
      auto p = bytes + 4 + i * 5;
      if (legacy && (p[0] >= 15 || p[1] + int(p[3]) > 12)) return false;
      next.objects[i] = {KitchenType(p[0]), p[1], KitchenLayer(p[2]), p[3], p[4]};
    }
    if (!next.valid()) return false;
    *this = next;
    return true;
  }
};
} // namespace leap
