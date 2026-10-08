#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace leap {
// Gameplay stays in world coordinates; the camera only selects the visible section.
struct CrabJourney {
  static constexpr int ViewWidth = 342, ViewHeight = 142, Capacity = 18;
  static constexpr float Speed = 92, CrabRadius = 6, CorridorHalf = 16;
  static constexpr int FrameMs = 40, BudgetBase = 3, BudgetGrowth = 2, BudgetMax = 28;
  enum Type { Urchin, Snail, Jelly, Kelp, Current, Whirlpool };
  struct Feature {
    int cost;
    float radius, amplitude, speed;
  };
  inline static constexpr Feature Features[] = {{1, 7, 0, 0},   {1, 5, 3, .22f}, {3, 7, 9, .7f},
                                                {2, 7, 2, .6f}, {3, 13, 0, .7f}, {5, 16, 0, .7f}};
  static constexpr float SuccessSeconds = 2.5f, ProtectionSeconds = 1.8f;
  static constexpr int ShorePoints = 8, WaterTop = 16, LevelsPerExpansion = 2, ExpansionWidth = 80;
  static constexpr int SignSpacing = 275;
  static constexpr float ShoreMargin = 3;
  static constexpr float UrchinRetreat = 45, KelpSpeedFactor = .35f;
  static constexpr float WhirlpoolPull = 44, WhirlpoolSwirl = 30;

  struct Element {
    Type type = Urchin;
    float x = 0, y = 0, radius = 0, amplitude = 0, speed = 0, phase = 0;
    bool horizontal = false;
    float px = 0, py = 0;
    float envelope() const {
      return radius + amplitude;
    }
  };
  struct Shell {
    float x = 0, y = 0;
    bool collected = false;
  };
  Element elements[Capacity]{};
  Shell shells[3]{};
  float shore[ShorePoints]{}, shoreX[ShorePoints]{}, route[6]{},
      x = 24, y = 78, worldWidth = ViewWidth, cameraX = 0;
  uint32_t seed = 1;
  unsigned level = 1;
  int count = 0, collected = 0, spent = 0;
  float age = 0, reaction = 0, protection = 0, celebration = 0, sparkle = 0;
  bool spinning = false, complete = false;
  unsigned expansions() const {
    return (level - 1) / LevelsPerExpansion;
  }
  unsigned signCount() const {
    // Preserve the first former goal; keep subsequent signs clear of the new goal.
    return expansions() ? 1 + unsigned((worldWidth - ViewWidth - ExpansionWidth) / SignSpacing) : 0;
  }
  static float signX(unsigned index) {
    return ViewWidth - 23 + float(index) * SignSpacing;
  }
  void followCamera() {
    // A dead zone avoids camera motion on small corrections and leaves room to look ahead.
    float screenX = x - cameraX;
    if (screenX > ViewWidth * .60f)
      cameraX = x - ViewWidth * .60f;
    else if (screenX < ViewWidth * .40f)
      cameraX = x - ViewWidth * .40f;
    cameraX = std::clamp(cameraX, 0.0f, worldWidth - ViewWidth);
  }
  uint32_t next() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
  }
  float random(float lo, float hi) {
    return lo + (next() % 10001) * (hi - lo) / 10000;
  }
  static float distance(float ax, float ay, float bx, float by) {
    return std::hypot(ax - bx, ay - by);
  }
  // Smooth, monotone interpolation keeps coves rounded without overshooting their bounds.
  float shoreY(float xx) const {
    xx = std::clamp(xx, 0.0f, worldWidth);
    int i = 0;
    while (i < ShorePoints - 2 && xx > shoreX[i + 1])
      ++i;
    float t = (xx - shoreX[i]) / (shoreX[i + 1] - shoreX[i]);
    t = t * t * (3 - 2 * t);
    return shore[i] + (shore[i + 1] - shore[i]) * t;
  }
  bool waterAt(float xx, float yy) const {
    return yy >= WaterTop && yy < shoreY(xx);
  }
  static bool aquatic(Type type) {
    return type == Jelly || type == Whirlpool || type == Current;
  }
  static float waterRadius(const Element &e) {
    // Includes the widest pulsing bell, tentacles, current arrows and whirlpool influence.
    return e.type == Jelly ? 14 : e.type == Current ? 18 : e.radius;
  }
  float waterBottom(float x, float radius) const {
    float minShore = std::min(shoreY(x - radius), shoreY(x + radius));
    // A monotone shore segment has its minimum at an endpoint or a control point.
    for (int i = 0; i < ShorePoints; ++i) {
      float xx = shoreX[i];
      if (xx >= x - radius && xx <= x + radius)
        minShore = std::min(minShore, shore[i]);
    }
    return minShore;
  }
  bool waterPlacement(const Element &e) const {
    float r = waterRadius(e) + e.amplitude;
    return e.y - r >= WaterTop + ShoreMargin && e.y + r + ShoreMargin <= waterBottom(e.x, r);
  }
  float routeY(float xx) const {
    float p = std::clamp((xx - 24) / (worldWidth - 48) * 5, 0.0f, 5.0f);
    int i = std::min(4, int(p));
    return route[i] + (route[i + 1] - route[i]) * (p - i);
  }
  // Distance to every segment protects a connected tube, including bends and swept motion.
  float routeDistance(float xx, float yy) const {
    float best = 10000;
    for (int i = 0; i < 5; ++i) {
      float ax = 24 + i * (worldWidth - 48) / 5, bx = 24 + (i + 1) * (worldWidth - 48) / 5;
      float dx = bx - ax, dy = route[i + 1] - route[i];
      float t =
          std::clamp(((xx - ax) * dx + (yy - route[i]) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
      best = std::min(best, distance(xx, yy, ax + t * dx, route[i] + t * dy));
    }
    return best;
  }
  bool placement(const Element &e) const {
    float r = e.envelope();
    if (aquatic(e.type) && !waterPlacement(e))
      return false;
    if (e.x - r < 62 || e.x + r > worldWidth - 44 || e.y - r < 25 || e.y + r > 138 ||
        routeDistance(e.x, e.y) < r + CrabRadius + CorridorHalf)
      return false;
    for (int i = 0; i < count; ++i)
      if (distance(e.x, e.y, elements[i].x, elements[i].y) < r + elements[i].envelope() + 5)
        return false;
    return true;
  }
  static int cost(Type t) {
    return Features[t].cost;
  }
  void start(unsigned difficulty, uint32_t randomSeed) {
    level = std::max(1u, difficulty);
    worldWidth = ViewWidth + float(expansions()) * ExpansionWidth;
    cameraX = 0;
    seed = randomSeed ? randomSeed : 1;
    age = reaction = protection = celebration = sparkle = 0;
    spinning = complete = false;
    count = collected = spent = 0;
    route[0] = route[5] = 78;
    for (int i = 1; i < 5; ++i)
      route[i] = random(56, 110);
    x = 24;
    y = route[0];
    // Mix the level into the coastline seed: even a reused preview seed gets a new coast.
    seed ^= level * 0x9e3779b9u;
    if (!seed)
      seed = 1;
    shoreX[0] = 0;
    shoreX[1] = worldWidth * .16f;
    shoreX[2] = worldWidth * random(.22f, .28f);
    shoreX[3] = worldWidth * random(.43f, .50f);
    shoreX[4] = worldWidth * random(.57f, .63f);
    shoreX[5] = worldWidth * random(.77f, .80f);
    shoreX[6] = worldWidth * .87f;
    shoreX[7] = worldWidth;
    shore[0] = shore[ShorePoints - 1] = 38;
    shore[1] = random(38, 48);
    shore[6] = random(38, 48);
    // Shift both bay location and width, with headlands instead of a fixed central bowl.
    // A broad deep section leaves room for aquatic features beside the protected route.
    float bayMin = level == 3 || level >= 5 ? 136 : 84;
    // Cycle the profile family per level; the seed chooses the initial family.
    switch ((randomSeed + level) % 4) {
    case 0: // Left bay.
      shore[2] = random(bayMin, 138);
      shore[3] = random(bayMin, 138);
      shore[4] = random(60, 95);
      shore[5] = random(50, 95);
      break;
    case 1: // Right bay.
      shore[2] = random(50, 95);
      shore[3] = random(60, 95);
      shore[4] = random(bayMin, 138);
      shore[5] = random(bayMin, 138);
      break;
    case 2: // Two bays separated by a headland.
      shore[2] = random(bayMin, 138);
      shore[3] = random(bayMin, 138);
      shore[4] = random(65, 95);
      shore[5] = random(110, 138);
      break;
    default: // Broad, asymmetric bay.
      shore[2] = random(100, 130);
      shore[3] = random(bayMin, 138);
      shore[4] = random(bayMin, 138);
      shore[5] = random(80, 118);
      break;
    }

    Type pool[3] = {Urchin, Urchin, Urchin};
    int types = 1;
    // Introduce each feature, then select only two or three types in later worlds.
    if (level > 1) {
      pool[1] = Type(std::min(5u, level - 1));
      types = 2;
    }
    if (level > 6) {
      pool[1] = Type(1 + next() % 5);
      pool[2] = Type(1 + next() % 5);
      types = 3;
    }
    int budget = std::min(BudgetMax, BudgetBase + int(std::min(level, 100u) - 1) * BudgetGrowth);
    for (int attempt = 0; attempt < 600 && count < Capacity; ++attempt) {
      // Prioritize the newly introduced type so it is actually visible when space permits.
      Type t = attempt < 400 && types > 1 && count == 0 ? pool[1] : pool[next() % types];
      if (spent + cost(t) > budget)
        continue;
      Element e;
      e.type = t;
      e.x = random(80, worldWidth - 62);
      e.y = random(38, 126);
      e.radius = Features[t].radius;
      e.amplitude = t == Jelly ? random(3, Features[t].amplitude) : Features[t].amplitude;
      e.speed = Features[t].speed;
      if (t == Jelly)
        e.speed += random(-.15f, .15f) + std::min(level, 12u) * .025f;
      e.phase = random(0, 6.28f);
      e.horizontal = next() % 2;
      if (aquatic(t)) {
        float r = waterRadius(e) + e.amplitude;
        float low = std::max(25 + e.envelope(), WaterTop + r + ShoreMargin);
        float high = std::min(138 - e.envelope(), waterBottom(e.x, r) - r - ShoreMargin - .01f);
        if (high < low)
          continue;
        // Favor the upper/lower side of a bay, away from the protected crossing.
        float inset = random(0, std::min(2.0f, (high - low) * .5f));
        e.y = next() % 2 ? low + inset : high - inset;
      }
      if (!placement(e))
        continue;
      e.px = e.x;
      e.py = e.y;
      elements[count++] = e;
      spent += cost(t);
    }
    // First shell on the guaranteed path, two optional detours with collision clearance.
    for (int s = 0; s < 3; ++s) {
      float sx = 90 + s * (worldWidth - 150) / 3;
      shells[s] = {sx, routeY(sx), false};
      if (!s)
        continue;
      for (int attempt = 0; attempt < 80; ++attempt) {
        float sy = random(46, 119);
        bool clear = true;
        for (int i = 0; i < count; ++i) {
          const auto &e = elements[i];
          // Reserve a clear vertical spur from the main path to each optional shell.
          float nearestY = std::clamp(e.y, std::min(sy, routeY(sx)), std::max(sy, routeY(sx)));
          if (distance(sx, nearestY, e.x, e.y) < e.envelope() + CrabRadius + 9)
            clear = false;
        }
        if (clear) {
          shells[s].y = sy;
          break;
        }
      }
    }
  }
  void setback(bool whirl, float retreat = 23) {
    x = std::max(24.0f, x - (whirl ? 38 : retreat));
    y = routeY(x);
    reaction = whirl ? 0.75f : 0.45f;
    protection = ProtectionSeconds;
    spinning = whirl;
    followCamera();
  }
  // Returns 1 for a shell and 2 for success, for the existing audio queue.
  int update(float dt, uint8_t directions) {
    dt = std::clamp(dt, 0.0f, 0.05f);
    age += dt;
    reaction = std::max(0.0f, reaction - dt);
    protection = std::max(0.0f, protection - dt);
    sparkle = std::max(0.0f, sparkle - dt);
    if (complete) {
      celebration += dt;
      return 0;
    }
    for (int i = 0; i < count; ++i) {
      auto &e = elements[i];
      float offset = std::sin(age * e.speed + e.phase) * e.amplitude;
      e.px = e.x + (e.horizontal ? offset : 0);
      e.py = e.y + (e.horizontal ? 0 : offset);
    }
    if (reaction > 0)
      return 0;
    float dx = bool(directions & 8) - bool(directions & 4),
          dy = bool(directions & 2) - bool(directions & 1);
    float norm = std::max(1.0f, std::hypot(dx, dy));
    float moveSpeed = Speed;
    for (int i = 0; i < count; ++i) {
      const auto &e = elements[i];
      if (e.type == Kelp && distance(x, y, e.px, e.py) < e.radius + CrabRadius) {
        moveSpeed *= KelpSpeedFactor;
        break;
      }
    }
    x += dx / norm * moveSpeed * dt;
    y += dy / norm * moveSpeed * dt;
    for (int i = 0; i < count; ++i) {
      const auto &e = elements[i];
      float d = distance(x, y, e.px, e.py);
      if (e.type == Current && d < e.radius) {
        x += (e.horizontal ? 13 : -8) * dt;
        y += (e.horizontal ? -5 : 12) * dt;
      } else if (e.type == Whirlpool && d < e.radius && protection <= 0) {
        if (d < 4) {
          setback(true);
          break;
        }
        float strength = (1 - d / e.radius) * dt;
        float ux = (e.px - x) / d, uy = (e.py - y) / d;
        // Clockwise drift matches the drawn spiral; full-speed steering can beat the current.
        x += (ux * WhirlpoolPull + uy * WhirlpoolSwirl) * strength;
        y += (uy * WhirlpoolPull - ux * WhirlpoolSwirl) * strength;
      } else if (e.type != Whirlpool && e.type != Current && e.type != Kelp &&
                 d < e.radius + CrabRadius && protection <= 0) {
        if (e.type == Snail) {
          if (d < 0.01f) {
            x = e.px + e.radius + CrabRadius + 1;
            continue;
          }
          float n = d;
          x = e.px + (x - e.px) / n * (e.radius + CrabRadius + 1);
          y = e.py + (y - e.py) / n * (e.radius + CrabRadius + 1);
        } else {
          setback(false, e.type == Urchin ? UrchinRetreat : 23);
          break;
        }
      }
    }
    x = std::clamp(x, 18.0f, worldWidth - 20);
    y = std::clamp(y, 44.0f, 125.0f);
    followCamera();
    int event = 0;
    for (auto &s : shells)
      if (!s.collected && distance(x, y, s.x, s.y) < 12) {
        s.collected = true;
        ++collected;
        sparkle = 0.7f;
        event = 1;
      }
    if (x >= worldWidth - 32 && std::abs(y - route[5]) < 22) {
      complete = true;
      celebration = 0;
      return 2;
    }
    return event;
  }
};
} // namespace leap
