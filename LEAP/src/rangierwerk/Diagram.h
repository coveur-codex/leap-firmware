#pragma once
#include "Yard.h"
#include <algorithm>
#include <cstdlib>

namespace rangierwerk {

struct MapPoint {
  int x = 0, y = 0;
};
inline MapPoint operator-(MapPoint a, MapPoint b) { return {a.x - b.x, a.y - b.y}; }

struct VehicleLength {
  int rear = 8, front = 8;
};

// Screen projection of the existing graph. The server still uses the same
// nodes, tracks and timing. Only the display is free to space and route them.
class Diagram {
  std::array<MapPoint, MaxNodes> points{};
  Index ends[2]{None, None};

  static int blend(int a, int b, int fraction) {
    return (a * (Substeps - fraction) + b * fraction + Substeps / 2) / Substeps;
  }

  void branch(const Siding &s) {
    if (s.end == None)
      return;
    const auto at = points[s.turnout];
    const int direction = at.y == Upper ? 1 : -1;
    const Index nodes[]{s.inner, s.middle, s.end};
    for (int i = 0; i < 3; ++i) {
      const int shift = direction * 10 * (i + 1);
      points[nodes[i]] = {at.x + shift, at.y - shift};
    }
  }

public:
  static constexpr int Left = 86, Width = 342, Bottom = 124;
  static constexpr int Upper = 45, Lower = 77, Radius = (Lower - Upper) / 2;
  static constexpr int Substeps = 256, StraightSpacing = 19;

  void build(const Layout &layout) {
    points.fill({});
    const int columns = (layout.loopCount - 2) / 2;
    const int first = Left + Width / 2 - (columns - 1) * StraightSpacing / 2;
    for (int col = 0; col < columns; ++col) {
      const int x = first + col * StraightSpacing;
      // Stagger the two tracks so crossovers read as diagonals. The ends
      // converge symmetrically into the return curves.
      const int stagger = std::min(15, 5 * std::min(col, columns - 1 - col));
      points[layout.loop[col]] = {x - stagger, Upper};
      points[layout.loop[2 * columns - col]] = {x + stagger, Lower};
    }
    ends[0] = layout.loop[columns];
    ends[1] = layout.loop[layout.loopCount - 1];
    points[ends[0]] = {first + (columns - 1) * StraightSpacing + Radius, Upper + Radius};
    points[ends[1]] = {first - Radius, Upper + Radius};
    branch(layout.intake);
    for (const auto &s : layout.targets) branch(s);
    for (const auto &s : layout.depots) branch(s);
    for (const auto &s : layout.storage) branch(s);

    for (Index i = 0; i < layout.switchCount; ++i) {
      const Index n = layout.nodes[layout.switches[i]].links[2];
      if (!points[n].x) {
        const auto &links = layout.nodes[n].links;
        const auto a = points[links[0]], b = points[links[1]];
        points[n] = {(a.x + b.x) / 2, (a.y + b.y) / 2};
      }
    }
  }

  MapPoint at(Index n) const { return points[n]; }
  bool curved(Index a, Index b) const {
    return a == ends[0] || a == ends[1] || b == ends[0] || b == ends[1];
  }

  MapPoint along(Index from, Index to, int fraction) const {
    const MapPoint a = at(from), b = at(to);
    if (!curved(from, to))
      return {blend(a.x, b.x, fraction), blend(a.y, b.y, fraction)};
    if (from == ends[0] || from == ends[1])
      return along(to, from, Substeps - fraction);
    // A quarter-circle table keeps the rail and movement on the same curve.
    constexpr int dx[]{0, 6, 11, 15, Radius}, dy[]{0, 1, 5, 10, Radius};
    const int segment = std::min(fraction / 64, 3);
    const int part = (fraction - segment * 64) * 4;
    return {a.x + (b.x > a.x ? 1 : -1) * blend(dx[segment], dx[segment + 1], part),
            a.y + (b.y > a.y ? 1 : -1) * blend(dy[segment], dy[segment + 1], part)};
  }

  VehicleLength vehicleLength(const Layout &layout, Index atNode, MapPoint direction) const {
    const auto here = at(atNode);
    int front = 0, rear = 0;
    const bool horizontal = std::abs(direction.x) > 2 * std::abs(direction.y);
    for (Index next : layout.nodes[atNode].links) {
      if (next == None || curved(atNode, next))
        continue;
      const auto delta = at(next) - here;
      const int x = std::abs(delta.x), y = std::abs(delta.y);
      if (horizontal != (y == 0))
        continue;
      int &gap = delta.x * direction.x + delta.y * direction.y > 0 ? front : rear;
      const int distance = std::max(x, y) + 3 * std::min(x, y) / 8;
      gap = gap ? std::min(gap, distance) : distance;
    }
    if (!front && !rear)
      return {};
    if (!front)
      front = rear;
    if (!rear)
      rear = front;
    return {std::clamp((rear - 2) / 2, 5, 11), std::clamp((front - 2) / 2, 5, 11)};
  }
};

} // namespace rangierwerk
