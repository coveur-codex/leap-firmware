#include "CrabJourney.h"
#include <cassert>
#include <cstdio>
using leap::CrabJourney;
int main() {
  unsigned seen[6]{};
  bool deepBay = false, variedShore = false;
  for (unsigned level = 1; level <= 30; ++level)
    for (uint32_t seed = 1; seed <= 150; ++seed) {
      CrabJourney w;
      w.start(level, seed);
      assert(w.worldWidth == w.ViewWidth + ((level - 1) / 2) * 80);
      assert(w.cameraX == 0);
      assert(w.shoreY(24) < 50 && w.shoreY(w.worldWidth - 24) < 50);
      for (float xx = 0; xx <= w.worldWidth; xx += 1) {
        float coast = w.shoreY(xx);
        assert(coast >= 38 && coast <= 138);
        deepBay |= coast > 135;
        variedShore |= coast > w.shoreY(24) + 60;
      }
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
        if (w.aquatic(e.type)) {
          float swept = w.waterRadius(e) + e.amplitude;
          assert(e.y - swept >= w.WaterTop + w.ShoreMargin);
          // Independently sample the entire swept graphic/influence footprint, not just its centre.
          for (float xx = e.x - swept; xx <= e.x + swept; xx += .5f)
            assert(e.y + swept + w.ShoreMargin <= w.shoreY(xx) + .001f);
          assert(e.y + swept + w.ShoreMargin <= w.shoreY(e.x + swept) + .001f);
        }
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
        assert(w.cameraX >= 0 && w.cameraX <= w.worldWidth - w.ViewWidth);
        assert(w.x - w.cameraX >= 18 && w.x - w.cameraX <= w.ViewWidth - 20);
      }
      assert(w.complete);
    }
  assert(deepBay && variedShore);
  for (auto n : seen)
    assert(n > 0);
  CrabJourney a, b;
  a.start(5, 1);
  b.start(5, 2);
  assert(a.route[1] != b.route[1]);
  assert(a.shore[2] != b.shore[2]);
  // Reusing the same seed across levels must still produce visibly different coastlines.
  for (unsigned level = 1; level < 30; ++level) {
    a.start(level, 734);
    b.start(level + 1, 734);
    float difference = 0;
    for (int xx = 0; xx <= a.ViewWidth; ++xx)
      difference += std::abs(a.shoreY(xx) - b.shoreY(xx));
    assert(difference / a.ViewWidth > 3);
    CrabJourney repeat;
    repeat.start(level, 734);
    for (int xx = 0; xx <= a.ViewWidth; ++xx)
      assert(a.shoreY(xx) == repeat.shoreY(xx));
  }
  // A shoreline dip inside a footprint must be checked, even when both ends are water.
  for (auto &height : a.shore)
    height = 138;
  a.shore[3] = 40;
  a.shoreX[2] = a.shoreX[3] - 24;
  a.shoreX[4] = a.shoreX[3] + 24;
  CrabJourney::Element waterAnimal;
  waterAnimal.type = CrabJourney::Whirlpool;
  waterAnimal.x = a.shoreX[3];
  waterAnimal.y = 45;
  waterAnimal.radius = 16;
  assert(waterAnimal.y + waterAnimal.radius + a.ShoreMargin < a.shoreY(waterAnimal.x - 16));
  assert(waterAnimal.y + waterAnimal.radius + a.ShoreMargin < a.shoreY(waterAnimal.x + 16));
  assert(!a.waterPlacement(waterAnimal));
  // Width is stable within each group, independent of the level's scene seed.
  float previousWidth = 0;
  for (unsigned level = 1; level <= 100; ++level) {
    a.start(level, level * 73);
    if (level > 1)
      assert(a.worldWidth - previousWidth == (level % 2 == 1 ? 80 : 0));
    previousWidth = a.worldWidth;
    assert(a.signCount() == (level <= 2 ? 0 : 1 + unsigned((a.worldWidth - 422) / 275)));
    for (unsigned i = 0; i < a.signCount(); ++i) {
      assert(a.signX(i) == 319 + i * 275 && a.signX(i) <= a.worldWidth - 103);
      if (i > 0)
        assert(a.signX(i) - a.signX(i - 1) >= 250 && a.signX(i) - a.signX(i - 1) <= 300);
    }
  }
  a.start(10, 22);
  a.count = 0;
  while (a.x < a.worldWidth - 60) {
    float camera = a.cameraX;
    a.update(.04f, 8);
    assert(a.cameraX >= camera && a.cameraX - camera <= a.Speed * .04f + .001f);
    if (a.cameraX > 0)
      assert(a.x - a.cameraX >= a.ViewWidth * .4f - .001f);
  }
  assert(a.cameraX == a.worldWidth - a.ViewWidth);
  float idleCamera = a.cameraX;
  a.update(.04f, 1);
  a.update(.04f, 0);
  assert(a.cameraX == idleCamera);
  while (a.x > 24) {
    float camera = a.cameraX;
    a.update(.04f, 4);
    assert(a.cameraX <= camera && camera - a.cameraX <= a.Speed * .04f + .001f);
    assert(a.x - a.cameraX <= a.ViewWidth - 20);
  }
  assert(a.cameraX == 0);
  a.x = 280;
  a.followCamera();
  a.setback(true);
  assert(a.x - a.cameraX >= a.ViewWidth * .4f - .001f && a.reaction > 0);
  a.start(4, 22);
  a.count = 0;
  a.x = a.signX(0);
  a.y = 78;
  a.update(.04f, 0);
  assert(!a.complete); // The former goal is now just a waypoint.
  a.x = a.worldWidth - 32;
  assert(a.update(.04f, 0) == 2 && a.complete);
  assert(a.cameraX == a.worldWidth - a.ViewWidth);
  idleCamera = a.cameraX;
  a.update(.04f, 4);
  assert(a.cameraX == idleCamera);
  a.start(1, 22);
  assert(a.worldWidth == a.ViewWidth && a.cameraX == 0);
  // Longer worlds must still be traversable with the actual camera and simulation.
  for (unsigned level : {31u, 100u, 300u}) {
    a.start(level, 734);
    for (int step = 0; step < 5000 && !a.complete; ++step) {
      float yy = a.routeY(a.x + 4);
      a.update(.04f, 8 | (a.y < yy - 1.5f ? 2 : a.y > yy + 1.5f ? 1 : 0));
      assert(a.reaction == 0);
      assert(a.cameraX >= 0 && a.cameraX <= a.worldWidth - a.ViewWidth);
    }
    assert(a.complete);
  }
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
