#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace leap {
enum class WeatherKind { Clear, MainlyClear, PartlyCloudy, Overcast, Fog, Drizzle,
                         Rain, FreezingRain, Snow, Showers, SnowShowers, Thunder, Hail, Unknown };
inline WeatherKind weatherKind(int code) {
  switch (code) {
  case 0: return WeatherKind::Clear;
  case 1: return WeatherKind::MainlyClear;
  case 2: return WeatherKind::PartlyCloudy;
  case 3: return WeatherKind::Overcast;
  case 45: case 48: return WeatherKind::Fog;
  case 51: case 53: case 55: return WeatherKind::Drizzle;
  case 56: case 57: case 66: case 67: return WeatherKind::FreezingRain;
  case 61: case 63: case 65: return WeatherKind::Rain;
  case 71: case 73: case 75: case 77: return WeatherKind::Snow;
  case 80: case 81: case 82: return WeatherKind::Showers;
  case 85: case 86: return WeatherKind::SnowShowers;
  case 95: return WeatherKind::Thunder;
  case 96: case 99: return WeatherKind::Hail;
  default: return WeatherKind::Unknown;
  }
}
inline const char *weatherLabel(int code, bool day = true) {
  switch (weatherKind(code)) {
  case WeatherKind::Clear: return day ? "Sonnig" : "Klar";
  case WeatherKind::MainlyClear: return day ? "Meist sonnig" : "Meist klar";
  case WeatherKind::PartlyCloudy: return "Wolkig";
  case WeatherKind::Overcast: return "Bedeckt";
  case WeatherKind::Fog: return "Nebel";
  case WeatherKind::Drizzle: return "Nieselregen";
  case WeatherKind::Rain: return "Regen";
  case WeatherKind::FreezingRain: return "Eisregen";
  case WeatherKind::Snow: return "Schnee";
  case WeatherKind::Showers: return "Regenschauer";
  case WeatherKind::SnowShowers: return "Schneeschauer";
  case WeatherKind::Thunder: return "Gewitter";
  case WeatherKind::Hail: return "Gewitter/Hagel";
  default: return "Wetter unbekannt";
  }
}
// All geometry lives in a 64x64 coordinate system and scales to either weather card.
// Only display primitives are needed: no downloads, font glyphs, or image decoder.
template <class Canvas> class WeatherPainter {
  Canvas &canvas;
  int x, y, size;
  uint16_t background;
  int at(int value) const { return (value * size + 32) / 64; }
  void circle(int cx, int cy, int radius, uint16_t color) {
    canvas.fillCircle(x + at(cx), y + at(cy), at(radius), color);
  }
  void line(int ax, int ay, int bx, int by, uint16_t color) {
    canvas.drawLine(x + at(ax), y + at(ay), x + at(bx), y + at(by), color);
  }
  void rect(int ax, int ay, int width, int height, uint16_t color) {
    canvas.fillRect(x + at(ax), y + at(ay), at(width), at(height), color);
  }
  void triangle(int ax, int ay, int bx, int by, int cx, int cy, uint16_t color) {
    canvas.fillTriangle(x + at(ax), y + at(ay), x + at(bx), y + at(by), x + at(cx), y + at(cy), color);
  }
  void sun(bool day, int cx, int cy) {
    if (!day) {
      circle(cx, cy, 12, 0xffb1);
      circle(cx + 6, cy - 5, 11, background);
      line(48, 7, 48, 13, 0xffdf); line(45, 10, 51, 10, 0xffdf);
      circle(56, 20, 1, 0xffdf);
      return;
    }
    for (int ray = 0; ray < 8; ++ray) {
      double angle = ray * 3.141592653589793 / 4;
      line(cx + std::lround(15 * std::cos(angle)), cy + std::lround(15 * std::sin(angle)),
           cx + std::lround(20 * std::cos(angle)), cy + std::lround(20 * std::sin(angle)), 0xfdc0);
    }
    circle(cx, cy, 12, 0xfbc0); circle(cx, cy, 10, 0xff20); circle(cx - 3, cy - 3, 3, 0xffd4);
  }
  void cloud(bool dark, int dy = 0) {
    uint16_t edge = 0x6baf, fill = dark ? 0x94b7 : 0xe75e;
    circle(17, 32 + dy, 10, edge); circle(29, 25 + dy, 14, edge); circle(44, 31 + dy, 12, edge);
    rect(16, 31 + dy, 29, 13, edge);
    circle(17, 32 + dy, 8, fill); circle(29, 25 + dy, 12, fill); circle(44, 31 + dy, 10, fill);
    rect(16, 31 + dy, 29, 11, fill);
    line(17, 40 + dy, 44, 40 + dy, dark ? 0x7bd2 : 0xc67a);
    circle(26, 20 + dy, 3, dark ? 0xad7b : 0xffff);
  }
  void flake(int cx, int cy) {
    line(cx - 4, cy, cx + 4, cy, 0xb7ff);
    line(cx - 2, cy - 4, cx + 2, cy + 4, 0xb7ff);
    line(cx + 2, cy - 4, cx - 2, cy + 4, 0xb7ff);
    circle(cx, cy, 1, 0xffff);
  }
public:
  WeatherPainter(Canvas &canvas, int x, int y, int size, uint16_t background)
      : canvas(canvas), x(x), y(y), size(size), background(background) {}
  void draw(int code, bool day = true) {
    WeatherKind kind = weatherKind(code);
    if (kind == WeatherKind::Unknown) {
      canvas.drawCircle(x + at(32), y + at(30), at(18), 0x9d35);
      line(25, 22, 32, 19, 0xe75e); line(32, 19, 39, 24, 0xe75e);
      line(39, 24, 32, 32, 0xe75e); line(32, 32, 32, 36, 0xe75e); circle(32, 42, 2, 0xe75e);
      return;
    }
    if (kind == WeatherKind::Clear) { sun(day, 32, 30); return; }
    if (kind == WeatherKind::MainlyClear || kind == WeatherKind::PartlyCloudy ||
        kind == WeatherKind::Showers || kind == WeatherKind::SnowShowers)
      sun(day, 21, 21);
    if (kind == WeatherKind::MainlyClear) { cloud(false, 9); return; }
    bool storm = kind == WeatherKind::Thunder || kind == WeatherKind::Hail;
    cloud(kind == WeatherKind::Overcast || storm);
    if (kind == WeatherKind::Fog) {
      line(7, 47, 42, 47, 0xc67a); line(19, 52, 57, 52, 0xc67a); line(9, 57, 43, 57, 0xc67a);
    } else if (kind == WeatherKind::Drizzle) {
      for (int cx : {18, 30, 42}) { circle(cx, 49, 1, 0x5e5f); circle(cx - 2, 56, 1, 0x5e5f); }
    } else if (kind == WeatherKind::Snow || kind == WeatherKind::SnowShowers) {
      flake(17, 51); flake(32, 56); flake(47, 50);
    } else if (storm) {
      triangle(30, 43, 24, 54, 34, 51, 0xff20); triangle(24, 54, 34, 51, 27, 62, 0xff20);
      if (kind == WeatherKind::Hail) { circle(16, 51, 3, 0xdfff); circle(45, 55, 3, 0xdfff); }
      else { line(17, 47, 13, 56, 0x5e5f); line(47, 47, 43, 56, 0x5e5f); }
    } else if (kind == WeatherKind::Rain || kind == WeatherKind::Showers || kind == WeatherKind::FreezingRain) {
      for (int cx : {18, 31, 44}) {
        line(cx, 47, cx - 4, 55, 0x5e5f); line(cx + 1, 47, cx - 3, 55, 0x5e5f);
        circle(cx - 4, 55, 1, 0x5e5f);
      }
      if (kind == WeatherKind::FreezingRain) flake(49, 58);
      if (code == 65 || code == 82) { line(25, 56, 23, 61, 0x5e5f); line(39, 56, 37, 61, 0x5e5f); }
    }
  }
};
template <class Canvas> void drawWeatherIcon(Canvas &canvas, int code, bool day,
                                            int x, int y, int size, uint16_t background) {
  WeatherPainter<Canvas>(canvas, x, y, size, background).draw(code, day);
}
} // namespace leap
