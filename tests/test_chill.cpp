#include "Chill.h"
#include "ChillAssets.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) { return malloc(n); }
void RamAllocator::deallocate(void *p) { free(p); }
void *RamAllocator::reallocate(void *p, size_t n) { return realloc(p, n); }
Storage storage;
Media::~Media() {}
std::vector<std::string> paths;
bool Media::draw(Arduino_GFX &, const String &, const String &path, int, int, int, int, bool,
                 uint16_t, bool, bool, bool transparent) {
  assert(transparent);
  paths.push_back(path);
  return true;
}
} // namespace leap
JsonDocument manifest(const char *scene) {
  JsonDocument m;
  auto d = m["definition"];
  d["scene"] = scene;
  d["slider"]["min"] = 0;
  d["slider"]["max"] = 100;
  d["slider"]["default"] = 45;
  auto s = d["sprites"].add<JsonObject>();
  s["role"] = "animation";
  s["size"].add(72);
  s["size"].add(96);
  for (const char *p : {"flame0.png", "flame1.png"}) {
    s["frames"].add(p);
    auto f = m["files"].add<JsonObject>();
    f["path"] = p;
    f["sha256"] = "abcdef";
  }
  return m;
}
int main(int argc, char **argv) {
  ChillMotion model;
  for (auto scene : {ChillScene::Space, ChillScene::Fire, ChillScene::Snow}) {
    model.reset(scene, 100, UINT32_MAX - 100);
    assert(!model.sliderVisible(0));
    assert(!model.input(5, UINT32_MAX - 50));
    assert(model.sliderVisible(20));
    assert(!model.sliderVisible(5000));
    assert(model.input(-1000, 20));
    assert(model.value == 0);
    assert(model.input(1000, 30));
    assert(model.value == 100);
    for (uint32_t t = 0; t < 60000; t += 100) {
      model.step(t);
      assert(model.count() <= int(model.particles.size()));
      for (int i = 0; i < model.count(); ++i)
        assert(std::isfinite(model.particles[i].x) && std::isfinite(model.particles[i].y));
    }
  }
  model.reset(ChillScene::Space, 0, 0);
  float x = model.particles[0].x;
  model.step(100);
  assert(model.particles[0].x == x);
  model.reset(ChillScene::Snow, 0, 0);
  assert(model.count() == 0);
  auto fire = manifest("fire"), snow = manifest("snow");
  assert(validChill(fire["definition"], fire["files"]));
  auto bad = manifest("fire");
  bad["files"][1]["path"] = "missing.png";
  assert(!validChill(bad["definition"], bad["files"]));
  Chill chill;
  chill.begin();
  Arduino_GFX gfx;
  assert(chill.start("chill-fire", 1, fire, 0));
  chill.input(10, 50);
  chill.tick(1000);
  assert(Preferences::ints.empty());
  chill.tick(2050);
  assert(Preferences::ints["leap-chillfire"] == 55);
  chill.input(5, 2100);
  chill.close();
  assert(Preferences::ints["leap-chillfire"] == 60);
  assert(chill.start("chill-fire", 1, fire, 2200));
  assert(chill.motion.value == 60);
  chill.draw(gfx, 2200);
  chill.draw(gfx, 2500);
  assert(paths.size() == 2);
  assert(paths[0] != paths[1]);
  assert(chill.start("chill-snow", 1, snow, 2700));
  assert(chill.motion.value == 45);
  chill.input(-5, 2800);
  chill.close();
  assert(chill.start("chill-fire", 2, fire, 3000));
  assert(chill.motion.value == 60);
  assert(!chill.start("bad", 1, bad, 3100));
  assert(!chill.active());
  for (int i = 1; i < argc; ++i) {
    std::ifstream in(argv[i]);
    JsonDocument m;
    assert(!deserializeJson(m, in));
    assert(validChill(m["definition"], m["files"]));
    assert(chill.start(argv[i], 1, m, 4000));
    chill.draw(gfx, 4100);
    chill.close();
  }
  std::cout << "PASS: Chill limits, wraparound, calm motion, slider timeout, per-scene NVS, frames "
               "and manifest references\n";
}
