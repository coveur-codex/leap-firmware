#include "ChillMusic.h"
#include "IdlePolicy.h"
#include "PictureTransition.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace leap;
int main() {
  std::set<uint64_t> profiles;
  ChillMusic music;
  music.seed(0x1234567);
  for (auto profile :
       {MusicProfile::Fire, MusicProfile::Space, MusicProfile::Snow, MusicProfile::Photos}) {
    music.request(profile, 100);
    uint64_t hash = 0;
    int64_t power = 0, finalMinute = 0;
    int previous = 0, maxDelta = 0;
    for (unsigned sample = 0; sample < 1800 * 22050; ++sample) {
      if (sample == 10 * 22050)
        music.request(profile, 0);
      if (profile == MusicProfile::Photos && sample % 22050 == 0)
        music.cue(PictureCue(1 + (sample / 22050) % 5));
      int value = music.next();
      assert(std::abs(value) <= 24000);
      maxDelta = std::max(maxDelta, std::abs(value - previous));
      previous = value;
      assert(music.activeVoices() <= 8);
      hash = hash * 31 + uint16_t(value);
      power += int64_t(value) * value;
      if (sample >= 1740 * 22050)
        finalMinute += int64_t(value) * value;
    }
    assert(power > 100000 && finalMinute > 100000 && maxDelta < 5000);
    profiles.insert(hash);
  }
  assert(profiles.size() == 4);
  music.request(MusicProfile::Off, 0);
  for (int i = 0; i < 22050; ++i)
    music.next();
  assert(!music.active() && music.activeVoices() == 0 && music.next() == 0);
  // Same profile continues composing beyond the first minute instead of resetting a loop.
  music.request(MusicProfile::Fire, 50);
  uint64_t first = 0, second = 0;
  for (int i = 0; i < 2 * 60 * 22050; ++i) {
    auto &hash = i < 60 * 22050 ? first : second;
    hash = hash * 31 + uint16_t(music.next());
  }
  assert(first != second);
  PictureTransition t;
  assert(t.intervalFor(0) == 300000 && t.intervalFor(150) == 165000 && t.intervalFor(300) == 30000);
  t.reset(0, 200);
  assert(t.interval == 120000);
  // Offline: no fade or musical event even after 30 minutes.
  for (uint32_t now = 0; now < 1800000; now += 100)
    assert(t.tick(now, false, true) == PictureCue::None && t.brightness(now) == 255);
  for (int mode = 1; mode <= 4; ++mode)
    for (uint32_t now : {300000u, 1800000u}) {
      auto idle = IdlePolicy::evaluate(now, 0, true, false);
      assert(!idle.dim && !idle.lock);
      idle = IdlePolicy::evaluate(now, 0, false, false);
      assert(idle.dim && idle.lock);
    }
  t.reset(UINT32_MAX - 10000, 300);
  uint32_t due = t.lastStart + 30000;
  assert(t.prefetch(due - 15000, true));
  assert(t.tick(due - 1200, true, true) == PictureCue::Prepare);
  assert(t.tick(due, true, true) == PictureCue::FadeOut);
  assert(t.brightness(due) == 255 && t.brightness(due + 750) == 128);
  assert(t.tick(due + 1500, true, true) == PictureCue::Black);
  assert(t.brightness(due + 1600) == 0);
  assert(t.tick(due + 1800, true, true) == PictureCue::FadeIn && t.swap);
  assert(t.brightness(due + 1800) == 0);
  assert(t.tick(due + 3300, false, true) == PictureCue::Complete);
  assert(!t.swap && t.brightness(due + 3300) == 255);
  assert(PictureTransition::dim(0xffff, 0) == 0 && PictureTransition::dim(0xffff, 255) == 0xffff);
  for (unsigned n = 1; n < 60; ++n) {
    uint32_t start = due + n * 30000;
    assert(t.tick(start - 1200, true, true) == PictureCue::Prepare);
    assert(t.tick(start, true, true) == PictureCue::FadeOut);
    assert(t.tick(start + 1500, true, true) == PictureCue::Black);
    assert(t.tick(start + 1800, true, true) == PictureCue::FadeIn);
    assert(t.tick(start + 3300, false, true) == PictureCue::Complete);
  }
  t.adjust(300, due + 1800000); // overdue adjustment gives a 15 s prefetch window
  assert(t.tick(due + 1800000, true, true) == PictureCue::None);
  puts("PASS: four generative profiles, synthesis bounds, smooth exits, evolving phrases, picture "
       "cues/fades/rollover, 30 minute idle policy and repeated transitions");
}
