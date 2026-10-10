#include "DragonRunDraw.h"
#include <cmath>
namespace leap {
namespace {
// Clip every primitive to the game viewport, including entering/leaving objects.
struct Painter {
  Arduino_GFX &g;
  void rect(int x, int y, int w, int h, uint16_t c) {
    int right = std::min(DragonRun::Width, x + w), bottom = std::min(DragonRun::Height, y + h);
    x = std::max(0, x);
    y = std::max(0, y);
    if (right > x && bottom > y)
      g.fillRect(86 + x, y, right - x, bottom - y, c);
  }
  void circle(int x, int y, int r, uint16_t c) {
    for (int dy = -r; dy <= r; ++dy) {
      int half = int(std::sqrt(float(r * r - dy * dy)));
      rect(x - half, y + dy, half * 2 + 1, 1, c);
    }
  }
  void triangle(int x, int y, int half, int h, uint16_t c) {
    for (int dy = 0; dy < h; ++dy) {
      int spread = half * dy / std::max(1, h - 1);
      rect(x - spread, y + dy, spread * 2 + 1, 1, c);
    }
  }
  void text(int x, int y, const char *s, uint16_t c = 0xffff) {
    g.setTextSize(1);
    g.setTextColor(c);
    g.setCursor(86 + x, y);
    g.print(s);
  }
};
} // namespace
void drawDragonRun(Arduino_GFX &gfx, const DragonRun &r, int best, bool saving) {
  Painter p{gfx};
  const int biome = r.biome();
  const uint16_t sky[] = {0xa6ff, 0x324e, 0x514b}, far[] = {0x7d97, 0x328c, 0x720b};
  const uint16_t near[] = {0x5490, 0x21a8, 0x4907}, soil[] = {0x8b66, 0x4285, 0x39a5};
  const uint16_t grass[] = {0x8e49, 0x4429, 0xfc43};
  p.rect(0, 0, 342, 142, sky[biome]);
  p.circle(284, 37, 12, biome ? 0xff59 : 0xffad);
  // Clouds, mountains, castles and trees use different scroll speeds.
  int clouds = int(std::fmod(r.distance * .12f, 140.0f));
  for (int x = -clouds - 35; x < 370; x += 140) {
    p.circle(x, 39, 7, 0xef9e);
    p.circle(x + 10, 35, 10, 0xef9e);
    p.circle(x + 22, 39, 7, 0xef9e);
    p.rect(x - 4, 38, 29, 7, 0xef9e);
  }
  int mountains = int(std::fmod(r.distance * .23f, 130.0f));
  for (int x = -mountains; x < 440; x += 130) {
    p.triangle(x + 44, 45 + (biome == 2 ? 5 : 0), 71, 65, far[biome]);
    p.triangle(x + 44, 45 + (biome == 2 ? 5 : 0), 11, 12, biome == 2 ? 0xfd43 : 0xdf7c);
  }
  int castle = int(std::fmod(r.distance * .32f, 390.0f));
  int cx = 250 - castle;
  for (int shift : {0, 390}) {
    int x = cx + shift;
    p.rect(x, 73, 39, 35, near[biome]);
    p.rect(x - 6, 68, 12, 40, near[biome]);
    p.rect(x + 33, 65, 12, 43, near[biome]);
    for (int i = 0; i < 4; ++i)
      p.rect(x - 6 + i * 13, 62, 6, 12, near[biome]);
    p.rect(x + 15, 91, 9, 17, far[biome]);
    p.rect(x + 36, 78, 3, 5, 0xff39);
    p.rect(x - 2, 80, 3, 5, 0xff39);
  }
  int trees = int(std::fmod(r.distance * .48f, 95.0f));
  for (int x = -trees + 18; x < 365; x += 95) {
    p.rect(x - 2, 82, 5, 34, biome == 2 ? 0x28c4 : 0x6205);
    if (biome == 2) {
      p.triangle(x, 82, 22, 34, near[biome]);
      p.rect(x - 9, 104, 3, 3, 0xfc43);
    } else {
      p.triangle(x, 63, 15, 24, near[biome]);
      p.triangle(x, 75, 22, 28, near[biome]);
      p.triangle(x - 4, 73, 9, 15, grass[biome]);
    }
  }
  p.rect(0, 116, 342, 26, soil[biome]);
  p.rect(0, 116, 342, 4, grass[biome]);
  int ground = int(std::fmod(r.distance, 37.0f));
  for (int x = -ground; x < 350; x += 37) {
    p.rect(x, 124, 7, 2, 0x7245);
    p.rect(x + 19, 129, 4, 1, 0xbca8);
    if (!biome) {
      p.rect(x + 12, 113, 1, 4, 0x4c26);
      p.circle(x + 12, 112, 1, 0xff9f);
    }
  }
  for (const auto &o : r.objects)
    if (o.active) {
      auto b = o.box();
      int x = int(o.x), y = int(o.y), w = int(b.w), h = int(b.h);
      switch (o.kind) {
      case DragonRun::Kind::Rock:
        p.rect(x + 2, y + 5, w - 4, h - 5, 0x632c);
        p.rect(x + 5, y, w - 10, h, 0x8c71);
        p.rect(x + 4, y + 5, 13, 3, 0xbdd6);
        p.rect(x + 13, y + 9, 4, 7, 0x6b4d);
        break;
      case DragonRun::Kind::Pillar:
        p.rect(x + 3, y, w - 6, h, 0x8c71);
        p.rect(x, y, w, 5, 0xbdd6);
        p.rect(x, y + h - 5, w, 5, 0x632c);
        p.rect(x + 6, y + 7, 3, h - 14, 0xbdd6);
        p.rect(x + 11, y + 15, 7, 2, 0x632c);
        break;
      case DragonRun::Kind::Bat: {
        int flap = int(r.time * 10) % 2 ? 0 : 4;
        p.triangle(x + 5, y - 3 + flap, 6, 10 - flap, 0x69ae);
        p.triangle(x + 19, y - 3 + flap, 6, 10 - flap, 0x69ae);
        p.circle(x + 12, y + 6, 6, 0x49ac);
        p.triangle(x + 9, y - 2, 3, 7, 0x49ac);
        p.triangle(x + 15, y - 2, 3, 7, 0x49ac);
        p.rect(x + 9, y + 3, 2, 3, 0xffff);
        p.rect(x + 14, y + 3, 2, 3, 0xffff);
        p.rect(x + 10, y + 9, 4, 1, 0xfdbb);
        break;
      }
      case DragonRun::Kind::Wood:
      case DragonRun::Kind::TallWood:
        for (int i = 0; i < 3; ++i) {
          p.rect(x + i * 9, y + 3, 7, h - 3, 0xa326);
          p.triangle(x + i * 9 + 3, y, 3, 5, 0xdca9);
          p.rect(x + i * 9 + 2, y + 6, 1, h - 8, 0xe56c);
        }
        p.rect(x, y + 11, w, 4, 0x7224);
        p.rect(x, y + h - 7, w, 4, 0x7224);
        p.rect(x + 3, y + 12, 2, 2, 0xc618);
        p.rect(x + 21, y + 12, 2, 2, 0xc618);
        break;
      case DragonRun::Kind::Coin:
        p.circle(x + 5, y + 5, 5, 0xd500);
        p.circle(x + 5, y + 5, 3, 0xffc0);
        p.rect(x + 5, y + 2, 1, 6, 0xffff);
        break;
      case DragonRun::Kind::Gem:
        p.triangle(x + 5, y, 5, 5, 0x87ff);
        for (int dy = 0; dy < 6; ++dy)
          p.rect(x + dy, y + 5 + dy, 11 - dy * 2, 1, 0x351f);
        p.rect(x + 4, y + 2, 2, 3, 0xffff);
        break;
      }
    }
  // Friendly dragon: outline, wagging tail, little wing, horns, feet and smile.
  int x = DragonRun::PlayerX, feet = DragonRun::Ground - int(r.lift);
  int h = r.ducking ? 15 : 28, y = feet - h;
  const uint16_t outline = !r.alive && int(r.deadTime * 16) % 2 == 0 ? 0xffff : 0x22c7;
  int wag = int(r.time * 9) % 2, wing = int(r.time * 8) % 3;
  p.triangle(x - 9, feet - 13 - wag, 7, 10, outline);
  p.rect(x - 10, feet - 9 - wag, 19, 5, outline);
  p.rect(x - 8, feet - 8 - wag, 16, 3, 0x65cb);
  p.circle(x + 13, feet - (r.ducking ? 7 : 12), r.ducking ? 7 : 11, outline);
  p.circle(x + 13, feet - (r.ducking ? 7 : 12), r.ducking ? 6 : 10, 0x65cb);
  p.rect(x + 7, feet - 8, 15, 6, 0xc755);
  p.circle(x + 24, y + (r.ducking ? 7 : 9), r.ducking ? 7 : 9, outline);
  p.circle(x + 24, y + (r.ducking ? 7 : 9), r.ducking ? 6 : 8, 0x65cb);
  p.rect(x + 24, y + (r.ducking ? 8 : 10), 10, 6, outline);
  p.rect(x + 24, y + (r.ducking ? 8 : 10), 9, 4, 0x8e6e);
  p.triangle(x + 19, y - 3, 2, 7, 0xff7a);
  p.triangle(x + 27, y - 2, 2, 6, 0xff7a);
  p.circle(x + 27, y + 6, 3, 0xffff);
  p.rect(x + 28, y + 5, 2, 3, 0x10a3);
  p.rect(x + 32, y + 10, 1, 1, 0x22c7);
  p.rect(x + 27, y + 14, 5, 1, 0x22c7);
  p.rect(x + 23, y + 11, 2, 2, 0xfdbb);
  if (!r.ducking) {
    p.triangle(x + 9, feet - 25 - wing, 6, 15, outline);
    p.triangle(x + 9, feet - 23 - wing, 4, 12, 0xaffe);
    p.rect(x + 9, feet - 20, 1, 8, 0x65cb);
  } else
    p.rect(x + 5, feet - 10, 8, 3, 0xaffe);
  int stride = r.lift > 0 || !r.alive ? 0 : int(r.time * 12) % 2 ? 3 : -2;
  p.rect(x + 5 + stride, feet - 4, 6, 4, outline);
  p.rect(x + 19 - stride, feet - 4, 6, 4, outline);
  p.rect(x + 6 + stride, feet - 2, 5, 2, 0x8e6e);
  p.rect(x + 20 - stride, feet - 2, 5, 2, 0x8e6e);
  if (r.flame > 0) {
    auto f = r.fireBox();
    int fx = int(f.x), fy = int(f.y) + 8;
    p.circle(fx + 13, fy, 7, 0xf980);
    p.circle(fx + int(f.w) - 19, fy, 9, 0xfd20);
    p.circle(fx + int(f.w) - 11 + int(r.time * 30) % 5, fy - 2, 5, 0xff60);
    p.rect(fx, fy - 3, int(f.w) - 12, 7, 0xfd20);
    p.rect(fx, fy - 1, int(f.w) - 18, 3, 0xffd4);
  }
  for (const auto &part : r.particles)
    if (part.life > 0)
      p.rect(int(part.x), int(part.y), 2, 2, part.color);
  p.rect(0, 0, 342, 19, 0x1949);
  p.text(6, 6, "Drachenrennen", 0xbff4);
  gfx.setCursor(86 + 96, 6);
  gfx.printf("%06d  Best %06d", r.score(), std::max(best, r.score()));
  p.text(285, 6, r.cooldown <= 0 ? "Feuer OK" : "Feuer...");
  p.rect(0, 132, 342, 10, 0x1949);
  p.text(6, 134, "HOCH Sprung  RUNTER Ducken  MITTE Feuer", 0xdedb);
  if (r.time < 2.8f && r.alive)
    p.text(82, 24, "Sammle Schaetze!", 0xffff);
  if (!r.alive && r.deadTime >= .35f) {
    p.rect(63, 34, 226, 61, 0x1949);
    p.rect(65, 36, 222, 57, 0xff9b);
    p.text(119, 42, "Gut gemacht!", 0x1949);
    gfx.setTextColor(0x1949);
    gfx.setCursor(86 + 77, 58);
    gfx.printf("Punkte %d   Rekord %d", r.score(), std::max(best, r.score()));
    p.text(77, 78, saving ? "Rekord wird gespeichert..." : "Rechts MITTE: Noch einmal", 0x1949);
  }
}
} // namespace leap
