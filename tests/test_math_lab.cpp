#include "MathLab.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace leap;
std::mt19937 rng(5819);
uint32_t randomWord() {
  return rng();
}
void key(MathLab &lab, bool right, Key k) {
  lab.input({right, k, false});
}
int main() {
  for (int total = 0; total <= 100; ++total)
    for (int a = 0; a <= total; ++a) {
      assert(mathlab::decomposition(total, a, total - a));
      assert(!mathlab::decomposition(total, a, total - a + 1));
    }
  assert(!mathlab::decomposition(7, -1, 8));
  mathlab::Histogram histogram;
  for (int i = 0; i < 36000; ++i) {
    int a = mathlab::die(randomWord), b = mathlab::die(randomWord);
    assert(a >= 1 && a <= 6 && b >= 1 && b <= 6);
    histogram.add(a, b);
  }
  uint32_t sum = 0;
  for (auto n : histogram.bins) {
    assert(n > 0);
    sum += n;
  }
  assert(sum == histogram.throws);
  // Every generated stack is supported, bounded and has an exposed top.
  for (int d = 0; d < 3; ++d)
    for (int i = 0; i < 1000; ++i) {
      auto b = mathlab::generate(randomWord, d);
      assert(b.count() == 2 + d * 2 && b.differences(b) == 0);
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
          int h = b.heights[y * 3 + x];
          assert(h <= 1 + d);
          if (x >= b.size || y >= b.size)
            assert(h == 0);
          if (x)
            assert(h <= b.heights[y * 3 + x - 1]);
          if (y)
            assert(h <= b.heights[(y - 1) * 3 + x]);
        }
      auto changed = b;
      ++changed.heights[0];
      assert(b.differences(changed) == 1 && !(b == changed));
    }
  auto v = mathlab::vertices(10);
  assert(v[0].x == 0 && v[0].y == -10 && v[5].y == 10);
  for (auto &e : mathlab::edges)
    assert(e[0] >= 0 && e[0] < 7 && e[1] >= 0 && e[1] < 7);
  MathLab lab;
  key(lab, true, Key::Center);
  assert(lab.screen == MathLab::Screen::Settings);
  key(lab, true, Key::Center);
  assert(lab.screen == MathLab::Screen::Play);
  key(lab, false, Key::Center);
  assert(lab.screen == MathLab::Screen::Settings);
  key(lab, false, Key::Center);
  assert(lab.screen == MathLab::Screen::Activities);
  assert(lab.input({false, Key::Center, false}));
  for (int activity = 0; activity < 4; ++activity)
    for (int variant = 0; variant < (activity == 1 ? 4 : 3); ++variant)
      for (int difficulty = 0; difficulty < 4; ++difficulty) {
        lab.start();
        lab.activity = activity;
        lab.variant = variant;
        lab.difficulty = std::min(2, difficulty);
        if (activity == 1)
          lab.difficulty = difficulty;
        lab.screen = MathLab::Screen::Play;
        lab.newTask();
        if (activity == 1 && variant == 1)
          for (int part = 0; part <= lab.target; ++part) {
            lab.part = part;
            lab.answer = lab.target - part;
            lab.check();
            assert(lab.correct);
          }
        if (activity == 2) {
          lab.built = lab.original;
          lab.check();
          assert(lab.correct);
          ++lab.built.heights[0];
          lab.check();
          assert(!lab.correct);
        }
        if (activity == 3 && variant == 2) {
          int matches = 0;
          for (auto &b : lab.views)
            matches += b == lab.original;
          assert(matches == 1);
        }
        Arduino_GFX gfx;
        gfx.recordAll = true;
        gfx.recordTextBounds = true;
        for (bool feedback : {false, true}) {
          lab.feedback = feedback;
          lab.help = feedback;
          lab.draw(gfx);
          for (auto &rect : gfx.rects) {
            assert(rect.x >= 86 && rect.x + rect.w <= 428);
            assert(rect.y >= 0 && rect.y + rect.h <= 142);
          }
          gfx.rects.clear();
        }
      }
  lab.start();
  lab.activity = 1;
  lab.variant = 3;
  lab.difficulty = 3;
  lab.screen = MathLab::Screen::Play;
  lab.target = 100;
  lab.tens = 9;
  key(lab, true, Key::Right);
  assert(lab.tens == 10);
  lab.check();
  assert(lab.correct);
  Arduino_GFX hundred;
  hundred.recordAll = hundred.recordTextBounds = true;
  lab.draw(hundred);
  for (auto &r : hundred.rects)
    assert(r.x >= 86 && r.x + r.w <= 428 && r.y + r.h <= 142);
  lab.screen = MathLab::Screen::Activities;
  lab.activity = 2;
  key(lab, true, Key::Center);
  assert(lab.difficulty == 2);
  lab.screen = MathLab::Screen::Play;
  lab.activity = 2;
  lab.variant = 1;
  lab.preview = true;
  lab.previewAt = UINT32_MAX - 1000;
  lab.seconds = 3;
  lab.tick(1000);
  assert(lab.preview);
  lab.tick(2000);
  assert(!lab.preview);
  lab.variant = 0;
  lab.preview = true;
  lab.tick(99999);
  assert(lab.preview);
  std::cout << "PASS: Math Lab arithmetic, 3000 buildings, geometry, navigation, views, preview "
               "and display bounds\n";
}
