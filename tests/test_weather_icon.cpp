#include "WeatherIcon.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

struct Canvas {
  std::ostringstream shapes;
  int calls = 0;
  static std::string color(uint16_t c) {
    char out[8];
    std::snprintf(out, sizeof(out), "#%02x%02x%02x", ((c >> 11) & 31) * 255 / 31,
                  ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31);
    return out;
  }
  void point(int x, int y) { assert(x >= 0 && x < 64 && y >= 0 && y < 64); }
  void fillCircle(int x, int y, int r, uint16_t c) {
    point(x-r,y-r); point(x+r,y+r); ++calls;
    shapes << "<circle cx='" << x << "' cy='" << y << "' r='" << r << "' fill='" << color(c) << "'/>";
  }
  void drawCircle(int x, int y, int r, uint16_t c) {
    point(x-r,y-r); point(x+r,y+r); ++calls;
    shapes << "<circle cx='" << x << "' cy='" << y << "' r='" << r << "' fill='none' stroke='" << color(c) << "'/>";
  }
  void drawLine(int ax, int ay, int bx, int by, uint16_t c) {
    point(ax,ay); point(bx,by); ++calls;
    shapes << "<path d='M" << ax << " " << ay << "L" << bx << " " << by << "' stroke='" << color(c) << "'/>";
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    point(x,y); point(x+w,y+h); ++calls;
    shapes << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' fill='" << color(c) << "'/>";
  }
  void fillTriangle(int ax, int ay, int bx, int by, int cx, int cy, uint16_t c) {
    point(ax,ay); point(bx,by); point(cx,cy); ++calls;
    shapes << "<path d='M" << ax << " " << ay << "L" << bx << " " << by << "L" << cx << " " << cy << "Z' fill='" << color(c) << "'/>";
  }
};
int main(int argc, char **argv) {
  using leap::WeatherKind;
  assert(leap::weatherKind(0) == WeatherKind::Clear);
  assert(leap::weatherKind(45) == WeatherKind::Fog);
  assert(leap::weatherKind(56) == WeatherKind::FreezingRain);
  assert(leap::weatherKind(77) == WeatherKind::Snow);
  assert(leap::weatherKind(86) == WeatherKind::SnowShowers);
  assert(leap::weatherKind(99) == WeatherKind::Hail);
  assert(leap::weatherKind(-1) == WeatherKind::Unknown);
  assert(std::string(leap::weatherLabel(0, false)) == "Klar");
  std::ostringstream svg;
  svg << "<svg xmlns='http://www.w3.org/2000/svg' width='640' height='340'><rect width='100%' height='100%' fill='#10202c'/>";
  int index = 0;
  for (int code : {0,1,2,3,45,48,51,53,55,56,57,61,63,65,66,67,71,73,75,77,80,81,82,85,86,95,96,99,-1}) {
    for (int size : {35,48,64}) {
      Canvas canvas;
      leap::drawWeatherIcon(canvas,code,true,0,0,size,0x10e5);
      assert(canvas.calls > 3);
    }
    if (code == 48 || code == 53 || code == 55 || code == 57 || code == 63 || code == 66 || code == 67 ||
        code == 73 || code == 75 || code == 77 || code == 81 || code == 82 || code == 86 || code == 96) continue;
    Canvas canvas;
    leap::drawWeatherIcon(canvas,code,true,0,0,64,0x10e5);
    int x = (index % 8) * 80 + 8, y = (index / 8) * 105 + 5;
    svg << "<g transform='translate(" << x << " " << y << ")'>" << canvas.shapes.str()
        << "<text x='32' y='82' text-anchor='middle' fill='#d4e8e4' font-size='9'>" << leap::weatherLabel(code) << "</text></g>";
    ++index;
  }
  Canvas night, day;
  leap::drawWeatherIcon(night,0,false,0,0,64,0x10e5);
  leap::drawWeatherIcon(day,0,true,0,0,64,0x10e5);
  assert(night.shapes.str() != day.shapes.str());
  svg << "<g transform='translate(8 215)'>" << night.shapes.str() << "<text x='32' y='82' text-anchor='middle' fill='#d4e8e4' font-size='9'>Nacht</text></g></svg>";
  if (argc > 1) { std::ofstream output(argv[1]); output << svg.str(); }
  std::puts("PASS: WMO conditions, detailed day/night icons and bounds at card sizes");
}
