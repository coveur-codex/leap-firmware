#include "CrabJourney.h"
#include <cassert>
#include <cstdio>
using leap::CrabJourney;
int main() {
  unsigned seen[6]{};
  for (unsigned level = 1; level <= 30; ++level)
    for (uint32_t seed = 1; seed <= 150; ++seed) {
      CrabJourney w;
      w.start(level, seed);
      assert(w.count > 0 && w.count <= w.Capacity);
      assert(w.spent <= std::min(w.BudgetMax, w.BudgetBase + int(level - 1) * w.BudgetGrowth));
      for (int i = 0; i < w.count; ++i) {
        auto e = w.elements[i];
        ++seen[e.type];
        assert(w.routeDistance(e.x, e.y) >= e.envelope() + w.CrabRadius + w.CorridorHalf);
        assert(e.x - e.envelope() >= 62 && e.x + e.envelope() <= w.worldWidth - 44);
        for (int j = 0; j < i; ++j)
          assert(w.distance(e.x, e.y, w.elements[j].x, w.elements[j].y) >=
                 e.envelope() + w.elements[j].envelope() + 5);
        assert(unsigned(e.type) <= std::min(5u, level - 1));
      }
      if (level >= 2 && level <= 6) {
        bool introduced = false;
        for (int i = 0; i < w.count; ++i)
          introduced |= unsigned(w.elements[i].type) == level - 1;
        assert(introduced);
      }
      for (const auto &shell : w.shells) {
        for (int i = 0; i < w.count; ++i) {
          const auto &e = w.elements[i];
          float nearestY = std::clamp(e.y, std::min(shell.y, w.routeY(shell.x)),
                                      std::max(shell.y, w.routeY(shell.x)));
          assert(w.distance(shell.x, nearestY, e.x, e.y) >= e.envelope() + w.CrabRadius + 9);
        }
      }
      // Traverse actual continuous movement, not just a connectivity assertion.
      for (int step = 0; step < 1500 && !w.complete; ++step) {
        float yy = w.routeY(w.x + 4);
        uint8_t directions = 8;
        if (w.y < yy - 1.5f)
          directions |= 2;
        if (w.y > yy + 1.5f)
          directions |= 1;
        w.update(.04f, directions);
        assert(w.reaction == 0);
      }
      assert(w.complete);
    }
  for (auto n : seen)
    assert(n > 0);
  CrabJourney a, b;
  a.start(5, 1);
  b.start(5, 2);
  assert(a.route[1] != b.route[1]);
  a.start(1, 22);
  a.count = 0;
  a.x = 60;
  a.y = 78;
  a.update(.04f, 8);
  assert(a.x > 63 && a.x < 65);
  float stopped = a.x;
  a.update(.04f, 0);
  assert(a.x == stopped);
  a.elements[0] = {};
  a.elements[0].x = a.x;
  a.elements[0].y = a.y;
  a.elements[0].radius = 7;
  a.count = 1;
  a.update(.04f, 0);
  assert(a.reaction > 0 && a.protection > 0 && a.x < stopped);
  a.start(6, 33);
  a.count = 1;
  a.elements[0] = {};
  auto &e = a.elements[0];
  e.type = CrabJourney::Whirlpool;
  e.x = a.x = 150;
  e.y = a.y = 78;
  e.radius = 16;
  a.update(.04f, 0);
  assert(a.spinning && a.x == 112 && a.protection > 0);
  a.start(5, 44);
  a.count = 1;
  a.elements[0] = {};
  a.elements[0].type = CrabJourney::Current;
  a.elements[0].radius = 13;
  a.elements[0].x = a.x;
  a.elements[0].y = a.y;
  float oldY = a.y;
  a.update(.04f, 0);
  assert(a.y > oldY);
  a.start(1, 55);
  a.count = 0;
  a.x = a.shells[0].x;
  a.y = a.shells[0].y;
  assert(a.update(.04f, 0) == 1 && a.collected == 1);
  assert(a.update(.04f, 0) == 0);
  a.start(1, 66);
  a.x = a.worldWidth - 32;
  a.y = 78;
  assert(a.update(.04f, 0) == 2 && a.collected == 0);
  float endX = a.x;
  a.update(.04f, 4);
  assert(a.x == endX && a.celebration > 0);
  // Diagonals do not accelerate; opposite directions cancel.
  a.start(1, 77);
  a.count = 0;
  a.x = 70;
  a.y = 80;
  a.update(.04f, 10);
  assert(std::abs(a.distance(a.x, a.y, 70, 80) - a.Speed * .04f) < .001f);
  float ox = a.x, oy = a.y;
  a.update(.04f, 15);
  assert(a.x == ox && a.y == oy);
  for (auto type : {CrabJourney::Jelly, CrabJourney::Snail, CrabJourney::Kelp}) {
    a.start(4, 88);
    a.count = 1;
    a.x = 100;
    a.y = 78;
    a.elements[0] = {};
    auto &animal = a.elements[0];
    animal.type = type;
    animal.x = 103;
    animal.y = 78;
    animal.radius = 7;
    a.update(.04f, 0);
    if (type == CrabJourney::Jelly)
      assert(a.reaction > 0 && a.protection > 0);
    else
      assert(a.reaction == 0 && a.protection == 0 && a.x < 100);
  }
  // Pull grows closer to the centre, and the sampled animation stays inside its envelope.
  for (float radius : {6.0f, 12.0f}) {
    a.start(6, 99);
    a.count = 1;
    a.elements[0] = {};
    auto &v = a.elements[0];
    v.type = CrabJourney::Whirlpool;
    v.x = 100;
    v.y = a.y = 78;
    v.radius = 16;
    a.x = 100 + radius;
    a.update(.04f, 0);
    assert(std::abs((100 + radius - a.x) - 18 * (1 - radius / 16) * .04f) < .001f);
  }
  a.start(3, 111);
  for (int step = 0; step < 250; ++step) {
    a.update(.04f, 0);
    for (int i = 0; i < a.count; ++i) {
      const auto &moving = a.elements[i];
      assert(a.distance(moving.x, moving.y, moving.px, moving.py) <= moving.amplitude + .001f);
    }
  }
  std::puts(
      "Crab journey: 4500 fair worlds, continuous movement, effects and optional shells passed");
}
