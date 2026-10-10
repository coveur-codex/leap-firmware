#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace leap {
// Fixed-size offline simulation. Coordinates are relative to the 342x142 game area.
struct DragonRun {
  static constexpr int Width = 342, Height = 142, Ground = 116, PlayerX = 44;
  static constexpr uint32_t FrameMs = 33;
  static constexpr float Gravity = 225, JumpVelocity = -180;
  static constexpr float StartSpeed = 60, MaxSpeed = 150, FireReach = 64;
  static constexpr float JumpSeconds = -2 * JumpVelocity / Gravity;
  static constexpr float FireDuration = .42f, FireCooldown = .95f, RestartDelay = .65f;
  static constexpr int MaxScore = 999999;
  enum class Kind : uint8_t { Rock, Pillar, Bat, Wood, TallWood, Coin, Gem };
  struct Box {
    float x, y, w, h;
    bool overlaps(const Box &b) const {
      return x < b.x + b.w && x + w > b.x && y < b.y + b.h && y + h > b.y;
    }
  };
  struct Object {
    Kind kind = Kind::Coin;
    float x = 0, y = 0;
    bool active = false;
    bool treasure() const {
      return kind == Kind::Coin || kind == Kind::Gem;
    }
    bool burnable() const {
      return kind == Kind::Bat || kind == Kind::Wood || kind == Kind::TallWood;
    }
    Box box() const {
      switch (kind) {
      case Kind::Rock:
        return {x, y, 22, 18};
      case Kind::Pillar:
        return {x, y, 22, 32};
      case Kind::Bat:
        return {x, y, 24, 12};
      case Kind::Wood:
        return {x, y, 26, 24};
      case Kind::TallWood:
        return {x, y, 28, 40};
      default:
        return {x, y, 10, 10};
      }
    }
  };
  struct Particle {
    float x = 0, y = 0, vx = 0, vy = 0, life = 0;
    uint16_t color = 0;
  };
  Object objects[8]{};
  Particle particles[18]{};
  float lift = 0, velocity = 0, distance = 0, time = 0, speed = StartSpeed;
  float flame = 0, cooldown = 0, deadTime = 0, nextSpawn = 180;
  int bonus = 0;
  bool alive = true, ducking = false;
  uint32_t rng = 1, hazards = 0;

  uint32_t random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  void start(uint32_t seed) {
    *this = DragonRun{};
    rng = seed ? seed : 1;
  }
  int score() const {
    return std::min(MaxScore, int(std::min(distance / 10, float(MaxScore))) + bonus);
  }
  int biome() const {
    return std::min(2, score() / 350);
  }
  Box playerBox() const {
    // Tail, wing and horns are decorative; body/head share this readable hitbox.
    return {PlayerX + 2, Ground - lift - (ducking ? 15.0f : 28.0f), 28, ducking ? 15.0f : 28.0f};
  }
  Box fireBox() const {
    auto p = playerBox();
    return {p.x + p.w, p.y + (ducking ? 2 : 7), FireReach, 18};
  }
  bool jump() {
    if (!alive || lift > .01f || velocity < 0)
      return false;
    velocity = JumpVelocity;
    ducking = false;
    return true;
  }
  bool fire() {
    if (!alive || cooldown > 0)
      return false;
    flame = FireDuration;
    cooldown = FireCooldown;
    return true;
  }
  bool restartReady() const {
    return !alive && deadTime >= RestartDelay;
  }
  // Leave the full jump plus reaction time between hazard edges:
  // landing/body clearance, reaction time and the .95 s fire cooldown all fit.
  float safeGap() const {
    return speed * (JumpSeconds + .25f) + 36;
  }
  bool add(Kind kind, float x, float y) {
    for (auto &o : objects)
      if (!o.active) {
        o = {kind, x, y, true};
        return true;
      }
    return false;
  }
  void burst(float x, float y, uint16_t color) {
    int count = 0;
    for (auto &p : particles)
      if (p.life <= 0 && count++ < 6) {
        p = {x, y, float(int(random() % 65) - 32), -float(25 + random() % 60), .45f, color};
      }
  }
  void spawn() {
    Kind kind;
    // Start gently; unlock mixed sequences rather than overlapping hazards.
    unsigned choices = hazards < 3 ? 2 : score() < 100 ? 4 : 5;
    unsigned pick = random() % choices;
    kind = pick == 0   ? Kind::Rock
           : pick == 1 ? Kind::Wood
           : pick == 2 ? Kind::Bat
           : pick == 3 ? Kind::Pillar
                       : Kind::TallWood;
    float y = kind == Kind::Bat        ? 87
              : kind == Kind::Pillar   ? Ground - 32
              : kind == Kind::TallWood ? Ground - 40
              : kind == Kind::Wood     ? Ground - 24
                                       : Ground - 18;
    add(kind, Width + 4, y);
    ++hazards;
    // Optional treasure is placed above jumping hazards, or in the clear gap.
    if (random() % 3 != 0) {
      Kind treasure = random() % 4 == 0 ? Kind::Gem : Kind::Coin;
      bool airborne = kind != Kind::Bat;
      add(treasure, Width + (airborne ? 12 : 76), airborne ? 62 : 103);
    }
    nextSpawn = 28 + safeGap() + float(random() % 50);
  }
  void step(float dt, bool down) {
    time += dt;
    for (auto &p : particles)
      if (p.life > 0) {
        p.life = std::max(0.0f, p.life - dt);
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 160 * dt;
      }
    if (!alive) {
      deadTime += dt;
      return;
    }
    speed = StartSpeed + std::min(MaxSpeed - StartSpeed, distance / 650);
    float move = speed * dt;
    distance = std::min(distance + move, float(MaxScore * 10));
    cooldown = std::max(0.0f, cooldown - dt);
    ducking = down && lift <= .01f && velocity >= 0;
    if (lift > 0 || velocity < 0) {
      lift -= velocity * dt + .5f * Gravity * dt * dt;
      velocity += Gravity * dt;
      if (lift <= 0) {
        lift = 0;
        velocity = 0;
        ducking = down;
      }
    }
    nextSpawn -= move;
    if (nextSpawn <= 0)
      spawn();
    for (auto &o : objects)
      if (o.active) {
        o.x -= move;
        if (o.x + o.box().w < 0) {
          o.active = false;
          continue;
        }
        auto box = o.box();
        if (flame > 0 && o.burnable() && fireBox().overlaps(box)) {
          o.active = false;
          bonus = std::min(MaxScore, bonus + 15);
          burst(box.x + box.w / 2, box.y + box.h / 2, 0xfd20);
        } else if (playerBox().overlaps(box)) {
          if (o.treasure()) {
            bonus = std::min(MaxScore, bonus + (o.kind == Kind::Gem ? 30 : 10));
            o.active = false;
            burst(box.x, box.y, 0xffc0);
          } else {
            alive = false;
            flame = 0;
            deadTime = 0;
            burst(PlayerX + 16, Ground - lift - 15, 0xffff);
            break;
          }
        }
      }
    flame = std::max(0.0f, flame - dt);
  }
  void update(float seconds, bool down) {
    // A delayed display/sync never teleports obstacles through the player.
    if (!std::isfinite(seconds) || seconds <= 0)
      return;
    float remaining = std::min(seconds, .1f);
    while (remaining > .00001f) {
      float dt = std::min(remaining, 1.0f / 120);
      step(dt, down);
      remaining -= dt;
    }
  }
};
static_assert(sizeof(DragonRun) < 1024, "Keep runner state bounded on ESP32");
} // namespace leap
