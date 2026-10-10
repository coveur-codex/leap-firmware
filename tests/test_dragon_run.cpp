#include "DragonRun.h"
#include "DragonRunDraw.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace leap;
using Kind = DragonRun::Kind;
static DragonRun empty() {
  DragonRun r;
  r.start(42);
  r.nextSpawn = 100000;
  return r;
}
static void advance(DragonRun &r, float seconds, bool down = false) {
  for (int i = 0; i < int(std::round(seconds * 120)); ++i)
    r.update(1.0f / 120, down);
}
int main() {
  auto r = empty();
  assert(r.jump());
  assert(!r.jump());
  advance(r, .6f);
  assert(r.lift > 71 && r.lift <= 72.1f);
  assert(!r.jump());
  advance(r, .65f);
  assert(r.lift == 0 && r.velocity == 0);
  r.update(.02f, true);
  assert(r.ducking && r.playerBox().h == 15);
  r.update(.02f, false);
  assert(!r.ducking && r.playerBox().h == 28);
  assert(r.jump() && r.fire());
  advance(r, .1f, true);
  assert(r.lift > 0 && !r.ducking && r.flame > 0);
  assert(!r.fire());
  advance(r, 1);
  assert(r.fire());
  for (Kind kind : {Kind::Rock, Kind::Pillar, Kind::Wood, Kind::TallWood, Kind::Bat}) {
    r = empty();
    float y = kind == Kind::Bat ? 87 : DragonRun::Ground - DragonRun::Object{kind}.box().h;
    r.add(kind, 65, y);
    r.update(.01f, false);
    assert(!r.alive);
    r = empty();
    r.add(kind, 80, y);
    r.fire();
    r.update(.05f, false);
    if (DragonRun::Object{kind}.burnable())
      assert(!r.objects[0].active && r.bonus == 15 && r.alive);
    else
      assert(r.objects[0].active && r.bonus == 0);
  }
  r = empty();
  r.add(Kind::Bat, 65, 87);
  r.update(.02f, true);
  assert(r.alive);
  r.update(.01f, false);
  assert(!r.alive);
  for (Kind kind : {Kind::Coin, Kind::Gem}) {
    r = empty();
    r.add(kind, 65, 103);
    r.update(.01f, false);
    assert(r.alive && !r.objects[0].active && r.bonus == (kind == Kind::Gem ? 30 : 10));
  }
  r = empty();
  r.add(Kind::Rock, 65, 98);
  r.update(.01f, false);
  int score = r.score();
  float distance = r.distance;
  assert(!r.restartReady());
  advance(r, .7f);
  assert(r.restartReady());
  assert(r.score() == score && r.distance == distance && r.flame == 0);
  r.start(13);
  assert(r.alive && r.score() == 0 && r.cooldown == 0);
  r.update(std::numeric_limits<float>::quiet_NaN(), false);
  assert(r.distance == 0);
  r.update(10, false);
  assert(r.distance < 8);
  // Clear each entire fireproof hitbox at both speed limits.
  for (float speed : {78.0f, 150.0f})
    for (Kind kind : {Kind::Rock, Kind::Pillar, Kind::Wood})
      for (float timingError : {-.09f, 0.0f, .09f}) {
        r = empty();
        r.distance = (speed - 78) * 650;
        auto shape = DragonRun::Object{kind}.box();
        float crossing = (shape.w + r.playerBox().w) / speed;
        float lead = (DragonRun::JumpSeconds - crossing) / 2 + timingError;
        r.add(kind, r.playerBox().x + r.playerBox().w + speed * lead, DragonRun::Ground - shape.h);
        r.jump();
        advance(r, 1.3f);
        assert(r.alive);
      }
  // Generated encounters across all biomes, including the maximum speed.
  for (uint32_t seed = 1; seed <= 64; ++seed) {
    r.start(seed);
    r.distance = seed % 3 * 47000.0f;
    r.nextSpawn = 1;
    for (int frame = 0; frame < 24000; ++frame) {
      bool down = false;
      const DragonRun::Object *next = nullptr;
      for (const auto &o : r.objects)
        if (o.active && !o.treasure() && o.x + o.box().w > r.playerBox().x)
          if (!next || o.x < next->x)
            next = &o;
      if (next) {
        float gap = next->x - (r.playerBox().x + r.playerBox().w);
        if (next->kind == Kind::Bat)
          down = gap < r.speed * .6f;
        else if (next->kind == Kind::TallWood) {
          if (gap < 32 && r.cooldown == 0)
            r.fire();
        } else {
          float crossing = (next->box().w + r.playerBox().w) / r.speed;
          float lead = (DragonRun::JumpSeconds - crossing) / 2;
          if (gap <= r.speed * lead && gap > 0 && r.lift == 0 && r.velocity == 0)
            r.jump();
        }
      }
      r.update(1.0f / 120, down);
      if (!r.alive) {
        std::cerr << "Unreachable seed " << seed << " frame " << frame << '\n';
        assert(false);
      }
      for (const auto &a : r.objects)
        for (const auto &b : r.objects)
          if (a.active && b.active && !a.treasure() && !b.treasure() && a.x < b.x)
            assert(b.x - a.x - a.box().w >= 140);
    }
    assert(r.hazards > 80);
  }
  Arduino_GFX gfx;
  gfx.recordAll = true;
  r = empty();
  r.add(Kind::Bat, -5, 87);
  r.add(Kind::TallWood, 335, 76);
  r.fire();
  r.update(.04f, false);
  for (int biome = 0; biome < 3; ++biome) {
    r.bonus = biome * 350;
    gfx.rects.clear();
    drawDragonRun(gfx, r, 123);
    for (auto box : gfx.rects)
      assert(box.x >= 86 && box.y >= 0 && box.x + box.w <= 428 && box.y + box.h <= 142);
  }
  // Reproducible preview from the actual firmware drawing primitives.
  r = empty();
  r.add(Kind::Rock, 175, 98);
  r.add(Kind::Wood, 295, 92);
  r.add(Kind::Gem, 195, 65);
  gfx.rects.clear();
  drawDragonRun(gfx, r, 120);
  std::vector<uint16_t> pixels(428 * 142, 0x18e7);
  for (auto box : gfx.rects)
    for (int y = box.y; y < box.y + box.h; ++y)
      for (int x = box.x; x < box.x + box.w; ++x)
        pixels[y * 428 + x] = box.color;
  std::ofstream image("build/tests/dragon-run.ppm", std::ios::binary);
  image << "P6\n428 142\n255\n";
  for (uint16_t c : pixels) {
    char rgb[] = {char(((c >> 11) & 31) * 255 / 31), char(((c >> 5) & 63) * 255 / 63),
                  char((c & 31) * 255 / 31)};
    image.write(rgb, 3);
  }
  std::cout
      << "PASS: Dragon Run physics, reachability, fire, treasure, rendering and 64 seeded runs\n";
}
