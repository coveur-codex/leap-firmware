#pragma once
#include <cstddef>
#include <cstdint>
namespace leap {
// Versioned little-endian checkpoint, independent of struct padding and firmware layout.
struct CrabJourneyProgress {
  static constexpr size_t SaveSize = 16;
  static constexpr uint32_t MaxLevel = 65535;
  uint32_t level = 1, seed = 0;
  static uint32_t read(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  }
  static void write(uint8_t *p, uint32_t value) {
    for (int i = 0; i < 4; ++i)
      p[i] = uint8_t(value >> (i * 8));
  }
  static uint32_t checksum(const uint8_t *p) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < SaveSize - 4; ++i)
      hash = (hash ^ p[i]) * 16777619u;
    return hash;
  }
  void encode(uint8_t *p) const {
    p[0] = 'C';
    p[1] = 'J';
    p[2] = 1;
    p[3] = 0;
    write(p + 4, level);
    write(p + 8, seed);
    write(p + 12, checksum(p));
  }
  bool decode(const uint8_t *p, size_t size) {
    if (size != SaveSize || p[0] != 'C' || p[1] != 'J' || p[2] != 1 || p[3] != 0 ||
        read(p + 12) != checksum(p) || !read(p + 4) || read(p + 4) > MaxLevel || !read(p + 8))
      return false;
    level = read(p + 4);
    seed = read(p + 8);
    return true;
  }
};
} // namespace leap
