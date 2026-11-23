#include "Rangierwerk.h"
#include "AccentColor.h"
#include "DisplayText.h"
#include <algorithm>
#include <cstdlib>

namespace leap {
namespace {
using namespace rangierwerk;
constexpr int Left = 86, Width = 342;
constexpr uint16_t Ink = 0x1108, Paper = 0xffbd, Grass = 0x5beb, Meadow = 0x6c2d,
                   Rail = 0x9d35, Ballast = 0x5265, Amber = 0xff04, White = 0xffff;
constexpr const char *Levels[]{"Kleiner Bahnhof", "Rangiermeister", "Güterbahnhof", "Großer Rangierbahnhof"};
constexpr const char *Destinations[]{"Sägewerk", "Baustelle", "Kraftwerk", "Lagerhaus", "Tankdepot"};
constexpr uint16_t CargoColors[]{0xaa64, 0x8c71, 0x31ca, 0xf52b, 0x2d18};
constexpr uint16_t RoofColors[]{0xc344, 0xfba4, 0x6299, 0xa83a, 0x1a96};

struct Point { int x, y; };
Point point(const Node &n) { return {Left + 6 + n.x * 7 / 5, n.y}; }
void text(Arduino_GFX &gfx, int x, int y, const char *value, uint16_t color = Ink, int size = 1) {
  gfx.setTextColor(color);
  gfx.setTextSize(size);
  gfx.setCursor(x, y);
  printDisplayText(gfx, value);
}
void panel(Arduino_GFX &gfx, int x, int y, int w, int h) {
  gfx.fillRoundRect(x + 2, y + 2, w, h, 6, 0x31c6);
  gfx.fillRoundRect(x, y, w, h, 6, Paper);
}
void trainSprite(Arduino_GFX &gfx, int x, int y, Index cargo, uint16_t color,
                 bool vertical, bool positive, bool selected) {
  // Each pixel column has the same meaning in either orientation. The cabin and
  // nose orient the locomotive, while the silhouettes identify the five loads.
  auto pixel = [&](int dx, int dy, uint16_t value) {
    if (!positive) dx = -dx;
    gfx.drawPixel(x + (vertical ? -dy : dx), y + (vertical ? dx : dy), value);
  };
  auto fill = [&](int x0, int y0, int w, int h, uint16_t c) {
    for (int dy = y0; dy < y0 + h; ++dy)
      for (int dx = x0; dx < x0 + w; ++dx)
        pixel(dx, dy, c);
  };
  const bool engine = cargo == None;
  if (selected)
    gfx.drawCircle(x, y, 9, White);
  fill(-5, -3, 11, 7, Ink);
  fill(-4, -4, 2, 9, Ink);
  fill(2, -4, 2, 9, Ink);
  fill(-4, -2, 9, 5, engine ? color : CargoColors[cargo]);
  if (engine) {
    fill(-4, -2, 3, 5, 0xc77f);
    fill(-3, -1, 1, 3, 0x39cf);
    fill(5, -1, 1, 3, color);
    pixel(5, 0, White); pixel(2, 0, contrastInk(color));
  } else if (cargo == 0) {
    fill(-3, -2, 7, 1, 0xf6aa);
    fill(-3, 0, 7, 1, 0xf6aa);
    fill(-3, 2, 7, 1, 0xe48a);
    pixel(3, 0, Paper); pixel(3, -2, Paper);
  } else if (cargo == 1) {
    pixel(-3, 1, White); pixel(-2, 0, White); pixel(0, -2, Paper);
    fill(1, -1, 2, 2, 0xc618); pixel(3, 2, Paper);
  } else if (cargo == 2) {
    fill(-3, -1, 7, 3, 0x2107);
    pixel(-2, 0, 0x7bee); pixel(0, -2, 0x7bee); pixel(3, 1, 0x7bee);
  } else if (cargo == 3) {
    fill(-3, -2, 3, 4, Amber); fill(1, 0, 3, 3, 0xdc6f);
    pixel(-2, -1, Paper); pixel(2, 1, Paper);
  } else {
    fill(-3, -2, 7, 5, 0x9f1e);
    fill(-2, 0, 5, 1, White); pixel(1, -1, 0x1bc9);
  }
}
void building(Arduino_GFX &gfx, Point at, Track kind, Index cargo, bool chosen) {
  const int x = at.x + 9, y = at.y < 62 ? at.y - 6 : at.y + 1;
  const uint16_t roof = kind == Track::Target ? RoofColors[cargo] : kind == Track::Depot ? 0x294b : 0x73a6;
  if (chosen)
    gfx.fillRoundRect(x - 2, y - 2, 19, 15, 3, Amber);
  gfx.fillRect(x + 1, y + 4, 13, 7, 0xef12);
  gfx.fillTriangle(x, y + 5, x + 7, y, x + 15, y + 5, roof);
  gfx.fillRect(x + 6, y + 6, 4, 5, kind == Track::Depot ? Ink : roof);
  gfx.drawPixel(x + 3, y + 7, White);
  if (kind == Track::Target) {
    trainSprite(gfx, x + 7, y + 7, cargo, 0, false, true, false);
    if (cargo == 2)
      gfx.fillRect(x + 11, y - 1, 3, 4, roof);
  }
}
void track(Arduino_GFX &gfx, Point a, Point b) {
  // Ballast and two thin rails, with evenly spaced wooden ties.
  const bool vertical = std::abs(a.x - b.x) < std::abs(a.y - b.y);
  const int dx = vertical ? 1 : 0, dy = vertical ? 0 : 1;
  for (int i = -3; i <= 3; ++i)
    gfx.drawLine(a.x + i * dx, a.y + i * dy, b.x + i * dx, b.y + i * dy, Ballast);
  const int steps = std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) / 4;
  for (int i = 0; i <= steps; ++i) {
    const int x = steps ? a.x + (b.x - a.x) * i / steps : a.x;
    const int y = steps ? a.y + (b.y - a.y) * i / steps : a.y;
    gfx.drawLine(x - 3 * dx, y - 3 * dy, x + 3 * dx, y + 3 * dy, 0x72c4);
  }
  for (int i : {-2, 2})
    gfx.drawLine(a.x + i * dx, a.y + i * dy, b.x + i * dx, b.y + i * dy, Rail);
}
} // namespace

void drawRangierwerkIcon(Arduino_GFX &gfx, int x, int y, uint16_t accent) {
  trainSprite(gfx, x, y, None, accent, false, true, false);
}

void Rangierwerk::draw(Arduino_GFX &gfx, uint16_t accent) const {
  const uint32_t now = millis();
  gfx.fillRect(Left, 0, Width, 142, Grass);
  if (screen == Screen::Setup || !playing()) {
    for (int y : {23, 113}) {
      track(gfx, {Left + 4, y}, {422, y});
      for (int x : {101, 408}) {
        drawRangierwerkIcon(gfx, x, y, accent);
        if (y > 25) trainSprite(gfx, x, y, 0, 0, false, true, false);
      }
    }
    panel(gfx, 112, 9, 290, 120);
    drawRangierwerkIcon(gfx, 131, 27, accent);
    text(gfx, 146, 20, "Rangierwerk", Ink, 2);
    if (screen == Screen::Setup) {
      const char *rows[]{mode ? "< Gemeinsam >" : "< Allein >", Levels[level - 1], "Unbegrenzt"};
      char minutes[24];
      if (LocalRound::Minutes[duration]) {
        snprintf(minutes, sizeof(minutes), "%d Minuten", LocalRound::Minutes[duration]);
        rows[2] = minutes;
      }
      for (int i = 0; i < (mode ? 1 : 3); ++i) {
        const int y = 46 + 16 * i;
        if (field == i)
          gfx.fillRoundRect(124, y - 3, 261, 14, 3, Amber);
        text(gfx, 131, y, rows[i]);
      }
      text(gfx, 126, 109, "R Pfeile: wählen   R Mitte: Start");
    } else {
      const char *message = remote.phase == RangierPhase::Full ? "Alle vier Loks sind unterwegs." :
                            remote.phase == RangierPhase::Error ? "Bahnhof derzeit nicht erreichbar." :
                            "Verbindung zum Bahnhof ...";
      text(gfx, 126, 56, message);
      text(gfx, 126, 84, "L Mitte: Spielmodus wählen");
    }
    return;
  }

  const auto &layout = yard.layout;
  const auto &fleet = vehicles();
  const uint32_t thrown = onlineMode ? remote.switches : yard.thrown;
  const Order *active[MaxOrders]{};
  int count = 0;
  for (const Order &o : orders())
    if (o.open())
      active[count++] = &o;
  const Order *focused = count ? active[order % count] : nullptr;
  const Index goal = focused ? focused->cargo : None;

  Random scenery{layout.seed ^ 0x58aad42u};
  for (int x = Left + 19; x < 420; x += 28) {
    const int y = 58 + scenery.pick(9);
    gfx.fillCircle(x, y + 2, 5, Meadow);
    gfx.fillCircle(x - 1, y, 3, 0x3526);
    gfx.drawPixel(x - 2, y - 1, 0x9669);
    gfx.drawPixel(x + 7, y + 4, Paper);
  }
  for (Index n = 0; n < layout.count; ++n) {
    const auto &node = layout.nodes[n];
    const Point a = point(node);
    for (Index next : node.links)
      if (next < n)
        track(gfx, a, point(layout.nodes[next]));
    if (node.links[1] == None) {
      gfx.fillRect(a.x - 4, a.y - 1, 9, 3, 0xaeab);
      if (node.kind == Track::Target || node.kind == Track::Depot || node.kind == Track::Intake)
        building(gfx, a, node.kind, node.cargo, node.kind == Track::Target && goal == node.cargo);
    }
  }
  for (Index i = 0; i < layout.switchCount; ++i) {
    const Index at = layout.switches[i];
    const auto &node = layout.nodes[at];
    const auto a = point(node), b = point(layout.nodes[node.links[1 + ((thrown >> i) & 1)]]);
    const bool selected = i == turnout;
    gfx.drawLine(a.x, a.y, (a.x + b.x) / 2, (a.y + b.y) / 2, selected ? Amber : White);
    if (selected) {
      gfx.drawCircle(a.x, a.y, 7, Amber);
      gfx.fillCircle(a.x, a.y, 2, White);
    }
    if ((switched & (1u << i)) && now - switchedAt < 240)
      gfx.drawCircle(a.x, a.y, 4 + (now - switchedAt) / 60, White);
  }

  Point centers[MaxVehicles]{};
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto &sprite = animation[i];
    auto a = point(layout.nodes[v.node]);
    if (sprite.from < layout.count && sprite.at == v.node && sprite.id == v.id) {
      const int elapsedMs = std::min<uint32_t>(180, now - sprite.movedAt);
      const Point from = point(layout.nodes[sprite.from]);
      a.x = from.x + (a.x - from.x) * elapsedMs / 180;
      a.y = from.y + (a.y - from.y) * elapsedMs / 180;
    }
    centers[i] = a;
  }
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto a = centers[i];
    for (Index next : v.links)
      if (next < i && fleet[next].id && fleet[next].node < layout.count) {
        const auto b = centers[next];
        gfx.drawLine(a.x, a.y, b.x, b.y, now - animation[i].linkAt < 300 ? Amber : Ink);
      }
  }
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto a = centers[i], at = point(layout.nodes[v.node]);
    Index nose = v.anchor;
    bool invert = i < Engines && v.anchorDirection < 0;
    // A stationary wagon lying across a curve follows the active coupling side.
    if (i >= Engines) {
      for (Index neighbor : v.links)
        if (neighbor < MaxVehicles && fleet[neighbor].id && fleet[neighbor].node < layout.count)
          nose = fleet[neighbor].node;
    }
    if (i < Engines && invert) {
      const Index front = layout.route(v.node, v.anchor, thrown);
      if (front != None) {
        nose = front;
        invert = false;
      }
    }
    Point dir = nose < layout.count ? point(layout.nodes[nose]) : Point{at.x + 1, at.y};
    const int dx = (dir.x - at.x) * (invert ? -1 : 1), dy = (dir.y - at.y) * (invert ? -1 : 1);
    const bool vertical = std::abs(dy) > std::abs(dx);
    const uint16_t color = onlineMode && i < Engines ? remote.colors[i] : accent;
    const bool pulse = v.unloading || (i >= Engines && now - animation[i].bornAt < 300);
    if (pulse) {
      const int radius = v.unloading ? 5 + (UnloadMs - v.unloading) / 120 : 7;
      gfx.drawCircle(a.x, a.y, radius, v.unloading ? Amber : Paper);
      gfx.drawPixel(a.x + radius, a.y - 2, White);
    }
    trainSprite(gfx, a.x, a.y, v.cargo, color, vertical, vertical ? dy >= 0 : dx >= 0, i == self());
    if (onlineMode && i < Engines) {
      const int x = a.x + 7, y = std::max(1, a.y - 11);
      gfx.fillRoundRect(x - 1, y - 1, 8, 10, 2, Ink);
      char number[]{static_cast<char>('1' + i), 0};
      text(gfx, x, y, number, (remote.connected & (1u << i)) ? White : Rail);
    }
    if (now - animation[i].linkAt < 320)
      gfx.drawPixel(a.x + 6, a.y - 4, Amber);
  }

  gfx.fillRect(Left, 127, Width, 15, Ink);
  gfx.setTextSize(1);
  gfx.setTextColor(White);
  if (focused) {
    trainSprite(gfx, 99, 134, goal, 0, false, true, false);
    gfx.setCursor(111, 131);
    gfx.printf("%u/%u  > ", unsigned(focused->delivered), unsigned(focused->count));
    printDisplayText(gfx, Destinations[goal]);
    if (count > 1) {
      gfx.setCursor(224, 131);
      gfx.printf("%u/%u", unsigned(order % count + 1), unsigned(count));
    }
  }
  gfx.setTextColor(deliveryAt && now - deliveryAt < 750 ? Amber : White);
  gfx.setCursor(263, 131);
  gfx.printf("Geliefert %lu", static_cast<unsigned long>(delivered()));
  text(gfx, 337, 131, "L halten", Rail);
  const auto remaining = (onlineMode ? remote.remaining : local.remaining) / 1000;
  gfx.setCursor(389, 131);
  if (!onlineMode && local.unlimited) gfx.print("--:--");
  else gfx.printf("%lu:%02lu", static_cast<unsigned long>(remaining / 60),
                  static_cast<unsigned long>(remaining % 60));

  if (summary() || (onlineMode && !rangierwerkMultiplayer.online)) {
    panel(gfx, 145, 36, 224, 56);
    if (onlineMode && !rangierwerkMultiplayer.online) {
      text(gfx, 158, 49, "Verbindung wird erneuert");
      text(gfx, 158, 68, "L Mitte: Spielmodus wählen");
    } else {
      text(gfx, 158, 47, onlineMode ? "Nächster Bahnhof" : "Feierabend!", Ink, 2);
      gfx.setTextSize(1);
      gfx.setCursor(158, 73);
      gfx.printf("%lu Wagen%s", static_cast<unsigned long>(delivered()),
                 onlineMode ? "" : "   R Mitte: neue Karte");
    }
  } else if (screen == Screen::Players) {
    panel(gfx, 151, 19, 216, 101);
    text(gfx, 165, 25, "Lokomotiven", Ink, 2);
    for (Index i = 0; i < Engines; ++i) {
      if (!fleet[i].id)
        continue;
      const int y = 49 + 16 * i;
      trainSprite(gfx, 169, y, None, remote.colors[i], false, true, i == self());
      gfx.setTextSize(1);
      gfx.setTextColor(Ink);
      gfx.setCursor(186, y - 3);
      gfx.printf("%u  ", unsigned(i + 1));
      printDisplayText(gfx, remote.names[i]);
      if (!(remote.connected & (1u << i))) text(gfx, 324, y - 3, "Pause");
    }
  } else if (screen == Screen::Pause) {
    panel(gfx, 161, 29, 197, 83);
    const char *choices[]{"Weiter", onlineMode ? "Lok bergen" : "Neue Karte",
                         onlineMode ? "Lokomotiven" : "Lok bergen", "Verlassen"};
    for (int i = 0; i < 4; ++i) {
      if (i == pause)
        gfx.fillRoundRect(169, 35 + 17 * i, 179, 15, 3, Amber);
      text(gfx, 176, 39 + 17 * i, choices[i]);
    }
  } else if (screen == Screen::Couplings) {
    const auto train = consist();
    panel(gfx, 128, 28, 252, 73);
    text(gfx, 140, 35, "Kupplungen", Ink, 2);
    const auto safeCut = std::min(cut, std::max(0, int(train.size) - 2));
    for (Index i = 0; i < train.size; ++i) {
      const int x = 164 + i * 44;
      const auto id = train.vehicles[i];
      if (i + 1 < train.size) {
        gfx.drawLine(x + 6, 66, x + 38, 66, Ink);
        gfx.fillCircle(x + 22, 66, 4, i == safeCut ? Amber : Rail);
      }
      trainSprite(gfx, x, 66, fleet[id].cargo, accent, false, true, id == self());
    }
    text(gfx, 140, 85, train.size > 1 ? "R <> wählen   Mitte lösen   v zurück" :
                                                 "Kein Wagen   R unten: zurück");
  } else if (now - openedAt < 2300) {
    panel(gfx, 161, 54, 195, 17);
    text(gfx, 173, 59, "Weiche am Lokschuppen stellen");
  }
}
} // namespace leap
