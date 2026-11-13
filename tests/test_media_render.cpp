#include "Media.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <PNGdec.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
using namespace leap;
int main(int argc, char **argv) {
  assert(argc == 2);
  String root = argv[1];
  Arduino_GFX gfx;
  // PSRAM exhaustion must fail cleanly without falling back into WiFi's heap.
  fakeMediaFailAt = 0; // PNG workspace fails; release the JPEG workspace again.
  assert(!Media::beginWorker());
  assert(fakeMediaCapsFrees == 1);
  fakeMediaFailAt = int(fakeMediaAllocations.size()) + 1; // JPEG fails after PNG succeeds.
  assert(!Media::beginWorker());
  assert(fakeMediaCapsFrees == 2);
  fakeMediaFailAt = -1;
  assert(Media::beginWorker());
  assert(fakeMediaAllocations.size() == 6);
  assert(fakeMediaAllocations[4].bytes == sizeof(PNG));
  assert(fakeMediaAllocations[5].bytes == sizeof(JPEGDEC));
  for (const auto &allocation : fakeMediaAllocations)
    assert(allocation.caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  assert(Media::beginWorker() && fakeMediaAllocations.size() == 6); // allocate once
#ifdef LEAP_REAL_JPEG_TEST
  auto *red = Media::decode("tests/fixtures/media/red.jpg", "red.jpg", 32, 16);
  assert(red);
  for (int i = 0; i < 32 * 16; ++i)
    assert((red[i] >> 11) >= 28 && ((red[i] >> 5) & 63) <= 3 && (red[i] & 31) <= 3);
  free(red);
  std::cout << "PASS: actual JPEGDEC RGB565 decoding from PSRAM workspace\n";
#endif
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
  // Actual user background dimensions: fit 264x142 into the 256x142 scene.
  Media wideBackground;
  assert(Media::validate(root + "/264-4.png", "background_day.png"));
  assert(!wideBackground.draw(gfx, root + "/264-4.png", "background_day.png", 86, 0, 256, 142, true));
  runWorker();
  auto beforeWide = gfx.pixels;
  assert(wideBackground.draw(gfx, root + "/264-4.png", "background_day.png", 86, 0, 256, 142, true));
  assert(gfx.pixels[2 * 428 + 86] == 0); // Fitted image starts at (86,2), source pixel (0,0).
  assert(gfx.pixels[200] == beforeWide[200]); // Proportional fit leaves the scene's top margin.
  assert(gfx.pixels[139 * 428 + 200] == beforeWide[139 * 428 + 200]);
  assert(gfx.pixels[2 * 428 + 342] == beforeWide[2 * 428 + 342]); // Never draws into action strip.

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
