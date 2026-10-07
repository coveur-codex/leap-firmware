#include "CrabJourneyDraw.h"
namespace leap {
namespace {
constexpr uint16_t Ink = 0x194b, Sand = 0xddd0, Coral = 0xfaca, Red = 0xfaa8, White = 0xffff;
// All world drawing is clipped, including objects partially outside the scrolling viewport.
struct Brush {
  Arduino_GFX &g;
  int left;
  void pixel(int x, int y, uint16_t c) {
    if (x >= 0 && x < 342 && y >= 0 && y < 142)
      g.drawPixel(left + x, y, c);
  }
  void line(int x, int y, int xx, int yy, uint16_t c) {
    int dx = std::abs(xx - x), sx = x < xx ? 1 : -1, dy = -std::abs(yy - y), sy = y < yy ? 1 : -1,
        err = dx + dy;
    for (;;) {
      pixel(x, y, c);
      if (x == xx && y == yy)
        break;
      int e = err * 2;
      if (e >= dy) {
        err += dy;
        x += sx;
      }
      if (e <= dx) {
        err += dx;
        y += sy;
      }
    }
  }
  void oval(int x, int y, int rx, int ry, uint16_t c) {
    for (int dy = -ry; dy <= ry; ++dy) {
      int w = int(rx * std::sqrt(std::max(0.0f, 1 - float(dy * dy) / (ry * ry))));
      line(x - w, y + dy, x + w, y + dy, c);
    }
  }
  void shell(int x, int y, uint16_t c, int r = 7) {
    oval(x, y - 2, r, 5, c);
    for (int i = -2; i <= 2; ++i)
      line(x, y + 3, x + i * 2, y - 5, 0xcbd2);
    line(x - 3, y + 4, x + 3, y + 4, c);
  }
  void kelp(int x, int y, float t, int height) {
    for (int stem = -1; stem <= 1; ++stem) {
      int ox = x + stem * 5, px = ox, py = y;
      for (int h = 3; h < height; h += 3) {
        int xx = ox + int(std::sin(t + h * .18f + stem) * 3);
        line(px, py, xx, y - h, 0x2cce);
        line(px + 1, py, xx + 1, y - h, 0x5d72);
        if (h % 2)
          line(xx, y - h, xx + stem * 4 + 3, y - h - 4, 0x5d72);
        px = xx;
        py = y - h;
      }
    }
  }
  void sign(int x, int y) {
    oval(x, y + 2, 11, 2, 0x9b69);
    for (int dx = -2; dx <= 1; ++dx)
      line(x + dx, y, x + dx, y - 29, dx == 0 ? 0xbba9 : 0x6266);
    // Wooden arrow board, outlined and engraved; entirely drawn with primitives.
    for (int h = -6; h <= 6; ++h) {
      int tip = 18 - std::abs(h);
      line(x - 13, y - 24 + h, x + tip, y - 24 + h, 0x6266);
      if (std::abs(h) < 5)
        line(x - 11, y - 24 + h, x + tip - 2, y - 24 + h, 0xff79);
    }
    line(x - 7, y - 24, x + 9, y - 24, Ink);
    line(x + 5, y - 28, x + 9, y - 24, Ink);
    line(x + 5, y - 20, x + 9, y - 24, Ink);
    pixel(x - 10, y - 27, Ink);
    pixel(x - 10, y - 21, Ink);
  }
  void crab(int x, int y, float t, bool scared, bool spin) {
    int bob = int(std::sin(t * 4) * 1.4f);
    y += bob;
    oval(x, y + 6, 14, 3, 0xbd8c);
    for (int side : {-1, 1}) {
      for (int i = 0; i < 3; ++i) {
        int wiggle = int(std::sin(t * 7 + i) * 2);
        line(x + side * 7, y + i * 3 - 2, x + side * (13 + i), y + i * 3 + wiggle, Ink);
        line(x + side * (13 + i), y + i * 3 + wiggle, x + side * 16, y + i * 3 + 3, Red);
      }
      int lift = int(std::sin(t * 3 + side) * 2);
      line(x + side * 8, y - 2, x + side * 13, y - 7 - lift, Red);
      oval(x + side * 13, y - 8 - lift, 4, 4, Red);
      line(x + side * 13, y - 12 - lift, x + side * 13, y - 8 - lift, Ink);
      line(x + side * 5, y - 4, x + side * 6, y - 11, Red);
      oval(x + side * 6, y - 12, 3, 3, White);
      pixel(x + side * 6 + int(std::sin(t)), y - 12, Ink);
    }
    oval(x, y, 10, 6, Red);
    oval(x - 2, y - 2, 7, 3, Coral);
    if (scared)
      oval(x, y + 2, 2, 2, Ink);
    else {
      line(x - 3, y + 1, x, y + 3, Ink);
      line(x, y + 3, x + 3, y + 1, Ink);
    }
    if (spin)
      for (int i = 0; i < 5; ++i) {
        float a = t * 10 + i * 1.26f;
        pixel(x + int(std::cos(a) * 19), y + int(std::sin(a) * 13), White);
      }
  }
  void animal(const CrabJourney::Element &e, float t, int x, int y) {
    int r = int(e.radius);
    float scale = .85f + (y - 35) * .002f;
    if (e.type == CrabJourney::Urchin) {
      for (int i = 0; i < 16; ++i) {
        float a = i * .393f;
        line(x + int(std::cos(a) * 5), y + int(std::sin(a) * 5),
             x + int(std::cos(a) * (11 * scale)), y + int(std::sin(a) * 10), 0x69b0);
      }
      oval(x, y, 7, 6, 0x8ab5);
      pixel(x - 2, y - 1, White);
      pixel(x + 2, y - 1, White);
    } else if (e.type == CrabJourney::Snail) {
      oval(x, y + 1, 10, 3, 0x9e33);
      oval(x - 2, y - 5, 7, 7, 0xfdb1);
      int px = x - 2, py = y - 5;
      for (int i = 0; i < 28; ++i) {
        float a = i * .35f, rr = i * .2f;
        int xx = x - 2 + int(std::cos(a) * rr), yy = y - 5 + int(std::sin(a) * rr);
        line(px, py, xx, yy, 0xbb6e);
        px = xx;
        py = yy;
      }
      oval(x + 7, y - 2, 4, 4, 0x9e33);
      for (int side : {-1, 1}) {
        line(x + 7 + side, y - 4, x + 7 + side * 3, y - 9, 0x9e33);
        pixel(x + 7 + side * 3, y - 9, Ink);
      }
    } else if (e.type == CrabJourney::Jelly) {
      int pulse = int(std::sin(t * 3 + e.phase) * 2) + int(e.phase) % 2;
      uint16_t c = e.horizontal ? 0xca9a : 0x7d3c;
      for (int i = -2; i <= 2; ++i) {
        int px = x + i * 3, py = y;
        for (int h = 2; h < 13; h += 2) {
          int xx = x + i * 3 + int(std::sin(t * 4 + i + h * .4f) * 2);
          line(px, py, xx, y + h, c);
          px = xx;
          py = y + h;
        }
      }
      oval(x, y - 5, 10 + pulse, 8, c);
      line(x - 9, y, x + 9, y, 0xfddf);
      pixel(x - 3, y - 5, Ink);
      pixel(x + 3, y - 5, Ink);
      line(x - 1, y - 2, x + 1, y - 2, White);
    } else if (e.type == CrabJourney::Kelp)
      kelp(x, y + 6, t + e.phase, int(23 * scale));
    else if (e.type == CrabJourney::Whirlpool) {
      int px = x, py = y;
      for (int i = 1; i < 65; ++i) {
        float a = i * .27f + t * 2, rr = i * r / 65.0f;
        int xx = x + int(std::cos(a) * rr), yy = y + int(std::sin(a) * rr * .65f);
        line(px, py, xx, yy, i % 8 < 4 ? 0x45bb : 0x96bd);
        px = xx;
        py = yy;
      }
      oval(x, y, 2, 2, 0x2396);
      for (int i = 0; i < 5; ++i) {
        float a = t + i * 1.26f;
        pixel(x + int(std::cos(a) * r), y + int(std::sin(a) * r), White);
      }
    } else {
      for (int i = 0; i < 8; ++i) {
        int offset = int(t * 13 + i * 7) % 26 - 13,
            xx = x + (e.horizontal ? offset : (i % 3 - 1) * 6),
            yy = y + (e.horizontal ? (i % 3 - 1) * 6 : offset);
        line(xx, yy, xx + (e.horizontal ? 4 : -2), yy + (e.horizontal ? -1 : 4), 0x6ddd);
      }
    }
  }
};
} // namespace
void drawCrabJourney(Arduino_GFX &gfx, const CrabJourney &w, int left) {
  Brush b{gfx, left};
  float t = w.age;
  gfx.fillRect(left, 0, 342, 142, Sand);
  gfx.fillRect(left, 0, 342, 8, 0xa71f);
  gfx.fillRect(left, 8, 342, 8, 0x4ddb);
  int coast[CrabJourney::ViewWidth];
  for (int xx = 0; xx < CrabJourney::ViewWidth; ++xx) {
    coast[xx] = int(w.shoreY(xx + w.cameraX));
    int shore = coast[xx];
    gfx.fillRect(left + xx, CrabJourney::WaterTop, 1, shore - 12 - CrabJourney::WaterTop, 0x34d7);
    gfx.fillRect(left + xx, shore - 12, 1, 7, 0x4dba);
    gfx.fillRect(left + xx, shore - 5, 1, 5, 0x8dd6);
    gfx.fillRect(left + xx, shore, 1, std::min(4, CrabJourney::ViewHeight - shore), 0xe5d2);
    // The surf shimmers inside the fixed habitat boundary; it never exposes an animal to sand.
    if (std::sin(t * 2 + (xx + w.cameraX) * .09f) > -.2f)
      b.pixel(xx, shore - 1, 0xe7ff);
  }
  for (int i = 0; i < 18; ++i)
    b.line(i * 21, 11 + int(std::sin(t + i) * 2), i * 21 + 12, 11 + int(std::sin(t + i) * 2),
           0xb75f);
  // Texture is generated in world tiles, so scenery stays anchored while scrolling.
  int firstTile = std::max(0, int(std::floor(w.cameraX / CrabJourney::ViewWidth)));
  int lastTile = int(std::floor((w.cameraX + CrabJourney::ViewWidth) / CrabJourney::ViewWidth));
  for (int tile = firstTile; tile <= lastTile; ++tile) {
    uint32_t texture = w.seed ^ (uint32_t(tile) * 0x9e3779b9u);
    for (int i = 0; i < 125; ++i) {
      texture = texture * 1664525u + 1013904223u;
      float wx = tile * CrabJourney::ViewWidth + texture % CrabJourney::ViewWidth;
      int xx = int(std::floor(wx - w.cameraX));
      int yy = 43 + (texture >> 16) % 96;
      if (xx >= 0 && xx < CrabJourney::ViewWidth && yy >= coast[xx] + 4) {
        b.pixel(xx, yy, 0xcced);
        if (i % 7 == 0)
          b.oval(xx, yy, 2, 1, 0xbdf0);
      }
    }
  }
  int firstBubble = std::max(0, int(std::floor((w.cameraX - 12) / 49)));
  for (int i = firstBubble; 12 + i * 49 < w.cameraX + CrabJourney::ViewWidth; ++i) {
    float wx = 12 + i * 49;
    int xx = int(wx - w.cameraX), yy = 38 + int(std::fmod(t * 8 + i * 17, 91.0f));
    if (w.waterAt(wx, yy)) {
      b.pixel(xx, yy, 0xefff);
      if (w.waterAt(wx + 1, yy - 1))
        b.pixel(xx + 1, yy - 1, 0xefff);
    }
  }
  int firstPlant = std::max(0, int(std::floor((w.cameraX - 74) / 68)));
  for (int i = firstPlant; 64 + i * 68 < w.cameraX + CrabJourney::ViewWidth + 10; ++i) {
    int xx = int(64 + i * 68 - w.cameraX), yy = 138 - i % 2 * 4;
    if (i % 2)
      b.kelp(xx, yy, t + i, 12 + i % 4);
    else {
      b.line(xx, yy, xx, yy - 12, Coral);
      b.line(xx, yy - 5, xx - 5, yy - 9, Coral);
      b.line(xx, yy - 6, xx + 4, yy - 10, Coral);
      b.oval(xx - 5, yy - 10, 2, 2, Coral);
      b.oval(xx, yy - 13, 2, 2, Coral);
    }
  }
  b.shell(int(18 - w.cameraX), 113, 0xef5a, 5);
  b.kelp(int(42 - w.cameraX), 139, t, 13);
  // Milestones start at the former goal and recur every 275 pixels; only visible signs are drawn.
  int firstSign = std::max(
      0, int(std::floor((w.cameraX - CrabJourney::signX(0) - 20) / CrabJourney::SignSpacing)));
  for (unsigned i = unsigned(firstSign);
       i < w.signCount() && CrabJourney::signX(i) < w.cameraX + CrabJourney::ViewWidth + 20; ++i)
    b.sign(int(CrabJourney::signX(i) - w.cameraX), 133);
  int goal = int(w.worldWidth - 23 - w.cameraX), gy = int(w.route[5]);
  b.oval(goal, gy + 6, 17, 6, 0xb56b);
  b.shell(goal, gy, 0xfddd, 15);
  b.line(goal - 14, gy + 4, goal + 14, gy + 4, 0x946c);
  for (int side : {-1, 1}) {
    int xx = goal + side * 14;
    b.line(xx, gy + 7, xx, gy - 9, Coral);
    b.line(xx, gy - 3, xx + side * 5, gy - 8, Coral);
    b.oval(xx, gy - 10, 2, 3, Coral);
  }
  for (const auto &s : w.shells)
    if (!s.collected) {
      int xx = int(s.x - w.cameraX);
      b.oval(xx, int(s.y) + 5, 8, 2, 0xbd8c);
      b.shell(xx, int(s.y), 0xffbd);
      b.pixel(xx, int(s.y) - 10, White);
    }
  // Ground effects first; animals and the crab then sorted by their foot Y.
  int order[CrabJourney::Capacity + 1], n = 0;
  for (int i = 0; i < w.count; ++i) {
    auto &e = w.elements[i];
    if (e.type == CrabJourney::Current || e.type == CrabJourney::Whirlpool)
      b.animal(e, t, int(e.px - w.cameraX), int(e.py));
    else
      order[n++] = i;
  }
  order[n++] = w.count;
  auto foot = [&](int i) { return i == w.count ? w.y : w.elements[i].py; };
  for (int i = 1; i < n; ++i) {
    int v = order[i], j = i;
    while (j > 0 && foot(order[j - 1]) > foot(v)) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = v;
  }
  for (int j = 0; j < n; ++j) {
    int i = order[j];
    if (i == w.count)
      b.crab(int(w.x - w.cameraX), int(w.y), t, w.reaction > 0, w.spinning && w.reaction > 0);
    else
      b.animal(w.elements[i], t, int(w.elements[i].px - w.cameraX), int(w.elements[i].py));
  }
  if (w.sparkle > 0)
    for (int i = 0; i < 6; ++i) {
      float a = i * 1.05f + t * 2;
      b.pixel(int(w.x - w.cameraX) + int(std::cos(a) * 21), int(w.y) + int(std::sin(a) * 17),
              White);
    }
  gfx.setTextSize(1);
  gfx.setTextColor(Ink);
  gfx.setCursor(left + 6, 23);
  gfx.printf("Krabbenreise %u   Muscheln %d/3", w.level, w.collected);
  if (w.complete) {
    for (int i = 0; i < 16; ++i) {
      float phase = t * 2 + i * 1.4f;
      int xx = 80 + i * 12, yy = 42 + int(std::sin(phase) * 12);
      b.line(xx - 2, yy, xx + 2, yy, i % 2 ? Coral : White);
      b.line(xx, yy - 2, xx, yy + 2, i % 2 ? Coral : White);
    }
    gfx.fillRoundRect(left + 68, 48, 211, 43, 8, 0xff9b);
    gfx.setCursor(left + 84, 57);
    gfx.print("Geschafft! Gut gemacht!");
    gfx.setCursor(left + 84, 74);
    gfx.printf("%d/3 Muscheln - weiter geht's", w.collected);
  }
}
} // namespace leap
