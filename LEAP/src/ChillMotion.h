#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace leap {
enum class ChillScene { None, Space, Fire, Snow };
// No images or framebuffer: shared bounded particle simulation, time in seconds.
class ChillMotion {
  uint32_t rng = 0x4c454150, previous = 0, touched = 0;
  bool overlay = false;
  float random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 65535) / 65536.f;
  }

public:
  struct Particle {
    float x, y, depth, phase;
  };
  std::array<Particle, 72> particles{};
  ChillScene scene = ChillScene::None;
  int value = 45;
  float time = 0;
  void reset(ChillScene next, int level, uint32_t now) {
    scene = next;
    value = std::clamp(level, 0, 100);
    previous = now;
    time = 0;
    overlay = false;
    for (auto &p : particles)
      p = {random() * 428, random() * 142, .25f + random() * .75f, random() * 6.283f};
  }
  bool input(int delta, uint32_t now) {
    overlay = true;
    touched = now;
    int next = std::clamp(value + delta, 0, 100);
    bool changed = value != next;
    value = next;
    return changed;
  }
  bool sliderVisible(uint32_t now) const { return overlay && uint32_t(now - touched) < 4000; }
  int count() const {
    if (scene == ChillScene::Space)
      return 56;
    if (scene == ChillScene::Snow)
      return value ? 4 + value * 68 / 100 : 0;
    if (scene == ChillScene::Fire)
      return value ? 1 + value * 11 / 100 : 0;
    return 0;
  }
  void step(uint32_t now) {
    float dt = std::min(uint32_t(now - previous), uint32_t(200)) / 1000.f;
    previous = now;
    time += dt;
    for (int i = 0; i < count(); ++i) {
      auto &p = particles[i];
      if (scene == ChillScene::Space) {
        p.x -= dt * value * .26f * p.depth;
        if (p.x < -2) {
          p.x = 430;
          p.y = random() * 142;
        }
      } else if (scene == ChillScene::Snow) {
        p.y += dt * (7 + value * .12f) * p.depth;
        p.x += dt * std::sin(time * .7f + p.phase) * (2 + value * .025f) * p.depth;
        if (p.y > 144) {
          p.y = -2;
          p.x = random() * 428;
        }
        if (p.x < -2)
          p.x = 430;
        if (p.x > 430)
          p.x = -2;
      } else if (scene == ChillScene::Fire) {
        if (p.y < 28 || p.x < 182 || p.x > 246) {
          p.x = 192 + random() * 44;
          p.y = 102 + random() * 12;
        }
        p.y -= dt * (7 + value * .09f) * p.depth;
        p.x += dt * std::sin(time + p.phase) * 2;
      }
    }
  }
};
} // namespace leap
