#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace leap {
namespace mathlab {
using Random = uint32_t (*)();
// Rejection sampling avoids modulo bias, including for the independent dice.
inline uint32_t below(Random random, uint32_t n) {
  const uint32_t threshold = uint32_t(-n) % n;
  uint32_t value;
  do {
    value = random();
  } while (value < threshold);
  return value % n;
}
inline int die(Random random) {
  return 1 + below(random, 6);
}
inline bool decomposition(int total, int a, int b) {
  return a >= 0 && b >= 0 && a + b == total;
}
struct Building {
  std::array<uint8_t, 9> heights{};
  int size = 2;
  int count() const {
    int n = 0;
    for (auto h : heights)
      n += h;
    return n;
  }
  bool operator==(const Building &b) const {
    return size == b.size && heights == b.heights;
  }
  unsigned differences(const Building &b) const {
    unsigned mask = 0;
    for (int i = 0; i < 9; ++i)
      if (heights[i] != b.heights[i])
        mask |= 1u << i;
    return mask;
  }
};
inline Building generate(Random random, int difficulty, bool complex = false) {
  Building b;
  b.size = difficulty < 2 ? 2 : 3;
  int cubes = 2 + difficulty * 2 + (complex ? 2 : 0);
  // Monotone heights from back to front keep every stack's top visible.
  for (int k = 0; k < cubes; ++k) {
    int candidates[9], count = 0;
    for (int y = 0; y < b.size; ++y)
      for (int x = 0; x < b.size; ++x) {
        int i = y * 3 + x, h = b.heights[i];
        if (h >= 1 + difficulty)
          continue;
        if (x && b.heights[i - 1] < h + 1)
          continue;
        if (y && b.heights[i - 3] < h + 1)
          continue;
        candidates[count++] = i;
      }
    if (count)
      ++b.heights[candidates[below(random, count)]];
  }
  return b;
}
struct Point {
  int x, y;
};
inline Point project(int x, int y, int z, int scale) {
  return {(x - y) * scale, (x + y) * scale / 2 - z * scale};
}
// Six perimeter edges and three visible spokes of the same isometric cube.
inline std::array<Point, 7> vertices(int scale) {
  return {{project(0, 0, 1, scale), project(1, 0, 1, scale), project(1, 1, 1, scale),
           project(0, 1, 1, scale), project(1, 0, 0, scale), project(1, 1, 0, scale),
           project(0, 1, 0, scale)}};
}
inline constexpr int edges[9][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {1, 4},
                                    {4, 5}, {5, 6}, {6, 3}, {2, 5}};
struct Histogram {
  std::array<uint32_t, 11> bins{};
  uint32_t throws = 0;
  void add(int a, int b) {
    if (throws == UINT32_MAX)
      return; // Saturate without wrapping or changing old data.
    ++bins[a + b - 2];
    ++throws;
  }
};
inline int numberLimit(int level) {
  return level == 0 ? 5 : level == 1 ? 10 : level == 2 ? 20 : 100;
}
} // namespace mathlab
} // namespace leap
