#include "Media.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace leap;
int main(int argc, char **argv) {
  assert(argc == 2);
  String root = argv[1];
  Arduino_GFX gfx;
  assert(Media::beginWorker());
  Media background, animal;
  auto runWorker = [] {
    try {
      fakeTask(fakeContext);
    } catch (const FakeQueueDrained &) {
    }
  };
  assert(!background.draw(gfx, root + "/256-3.png", "background_day.png", 86, 0, 256, 142));
  runWorker();
  assert(background.draw(gfx, root + "/256-3.png", "background_day.png", 86, 0, 256, 142));
  auto saved = gfx.pixels;
  assert(gfx.pixels[50 * 428 + 200] != 0); // Actual decoded background, not fallback fill.
  assert(!animal.draw(gfx, root + "/80-alpha.png", "idle1.png", 174, 40, 80, 80, false, 0x10e5,
                      true, true, true));
  runWorker();
  assert(animal.draw(gfx, root + "/80-alpha.png", "idle1.png", 174, 40, 80, 80, false, 0x10e5, true,
                     true, true));
  assert(gfx.pixels[40 * 428 + 174] == saved[40 * 428 + 174]); // Transparent pixel preserves scene.
  assert(gfx.pixels[40 * 428 + 176] == 0);                     // Opaque black remains visible.
  // A new animation frame retains the previous decoded image until the worker completes.
  assert(animal.draw(gfx, root + "/80-3.png", "idle2.png", 174, 40, 80, 80, false, 0x10e5, true,
                     true, true));
  runWorker();
  assert(animal.draw(gfx, root + "/80-3.png", "idle2.png", 174, 40, 80, 80, false, 0x10e5, true,
                     true, true));
  assert(gfx.pixels[40 * 428 + 176] != 0);
  // A missing background can arrive on sync without changing its content-hash path.
  Media lateBackground;
  String late = root + "/late-background.dat";
  std::remove(late.c_str());
  assert(!lateBackground.draw(gfx, late, "background_day.png", 86, 0, 256, 142));
  runWorker();
  assert(!lateBackground.draw(gfx, late, "background_day.png", 86, 0, 256, 142));
  {
    std::ifstream source((root + "/256-3.png").c_str(), std::ios::binary);
    std::ofstream target(late.c_str(), std::ios::binary);
    target << source.rdbuf();
  }
  fakeNow += 1999;
  assert(!lateBackground.draw(gfx, late, "background_day.png", 86, 0, 256, 142));
  runWorker();
  fakeNow += 1;
  assert(!lateBackground.draw(gfx, late, "background_day.png", 86, 0, 256, 142));
  runWorker();
  assert(lateBackground.draw(gfx, late, "background_day.png", 86, 0, 256, 142));
  assert(gfx.pixels == saved);
  std::remove(late.c_str());
  std::cout << "PASS: real asynchronous Media.cpp PNG scene, transparent sprite and frame change\n";
}
