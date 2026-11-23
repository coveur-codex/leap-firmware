#include "Rangierwerk.h"
#include "AccentColor.h"
#include "DisplayText.h"
#include <algorithm>
#include <cstdlib>
#include <utility>

namespace leap {
namespace {
using namespace rangierwerk;
constexpr uint16_t rgb(unsigned hex) {
  return ((hex >> 19) << 11) | (((hex >> 10) & 63) << 5) | ((hex >> 3) & 31);
}
constexpr auto Ink = rgb(0x243543), Paper = rgb(0xf5f3eb), White = rgb(0xfffffc),
               Rail = rgb(0xa1aaa9), Active = rgb(0x5a6e75), Muted = rgb(0x5d6b73),
               Focus = rgb(0x076960), FocusWash = rgb(0xd3e9d9), Divider = rgb(0xdadcd3),
               Amber = rgb(0xf0bd52);
constexpr const char *Levels[]{"Kleiner Bahnhof", "Rangiermeister", "Güterbahnhof", "Großer Rangierbahnhof"};
constexpr const char *Destinations[]{"Sägewerk", "Baustelle", "Kraftwerk", "Lagerhaus", "Tankdepot"};
constexpr uint16_t CargoColors[]{rgb(0x9a5230), rgb(0x69677f), Ink, rgb(0xa75f13), rgb(0x116585)};

void text(Arduino_GFX &gfx, int x, int y, const char *value, uint16_t color = Ink, int size = 1) {
  gfx.setTextColor(color);
  gfx.setTextSize(size);
  gfx.setCursor(x, y);
  printDisplayText(gfx, value);
}
void panel(Arduino_GFX &gfx, int x, int y, int w, int h) {
  gfx.fillRoundRect(x - 1, y - 1, w + 2, h + 2, 7, Divider);
  gfx.fillRoundRect(x, y, w, h, 6, White);
}

void stroke(Arduino_GFX &gfx, MapPoint a, MapPoint b, uint16_t color, bool thick = true) {
  gfx.drawLine(a.x, a.y, b.x, b.y, color);
  if (thick) {
    const int dx = std::abs(b.x - a.x) < std::abs(b.y - a.y);
    const int dy = !dx;
    gfx.drawLine(a.x + dx, a.y + dy, b.x + dx, b.y + dy, color);
    gfx.drawLine(a.x - dx, a.y - dy, b.x - dx, b.y - dy, color);
  }
}

void track(Arduino_GFX &gfx, const Diagram &diagram, Index from, Index to) {
  if (!diagram.curved(from, to)) {
    stroke(gfx, diagram.at(from), diagram.at(to), Rail);
    return;
  }
  auto a = diagram.at(from);
  for (int t = 64; t <= Diagram::Substeps; t += 64) {
    const auto b = diagram.along(from, to, t);
    stroke(gfx, a, b, Rail);
    a = b;
  }
}

void cargoMark(Arduino_GFX &gfx, MapPoint at, Index cargo, uint16_t color) {
  const auto [x, y] = at;
  switch (cargo) {
  case 0:
    gfx.drawLine(x - 2, y - 2, x - 2, y + 2, color);
    gfx.drawLine(x, y - 2, x, y + 2, color);
    gfx.drawLine(x + 2, y - 2, x + 2, y + 2, color);
    break;
  case 1:
    gfx.fillTriangle(x, y - 3, x - 3, y, x + 3, y, color);
    gfx.fillTriangle(x, y + 3, x - 3, y, x + 3, y, color);
    break;
  case 2:
    gfx.fillCircle(x, y, 2, color);
    break;
  case 3:
    gfx.drawRect(x - 2, y - 2, 5, 5, color);
    gfx.drawPixel(x, y, color);
    break;
  case 4:
    gfx.drawCircle(x, y, 2, color);
    break;
  }
}

void terminal(Arduino_GFX &gfx, MapPoint at, Track kind, Index cargo, bool chosen) {
  const int side = at.y < Diagram::Upper ? 1 : -1;
  if (kind == Track::Storage) {
    gfx.fillCircle(at.x, at.y, 2, Active);
    return;
  }
  const MapPoint marker{at.x + side * 10, at.y - side * 7};
  stroke(gfx, at, marker, Rail, false);
  const auto [x, y] = marker;
  if (kind == Track::Target) {
    if (chosen)
      gfx.fillCircle(x, y, 8, FocusWash);
    gfx.fillCircle(x, y, 5, White);
    gfx.drawCircle(x, y, chosen ? 6 : 5, chosen ? Focus : CargoColors[cargo]);
    cargoMark(gfx, marker, cargo, CargoColors[cargo]);
  } else {
    gfx.fillRoundRect(x - 5, y - 4, 11, 9, 2, White);
    gfx.drawRect(x - 5, y - 4, 11, 9, Active);
    if (kind == Track::Depot) {
      gfx.drawRect(x - 2, y - 2, 5, 6, Active);
    } else {
      gfx.drawLine(x - 3, y, x + 3, y, Focus);
      gfx.drawLine(x + 1, y - 2, x + 3, y, Focus);
      gfx.drawLine(x + 1, y + 2, x + 3, y, Focus);
    }
  }
}

struct Pose {
  MapPoint at;
  MapPoint unit{256, 0}; // Signed direction in fixed point.
  VehicleLength length{};

  MapPoint position(int along, int across = 0) const {
    auto pixels = [](int value) { return (value + (value < 0 ? -128 : 128)) / 256; };
    return {at.x + pixels(along * unit.x - across * unit.y),
            at.y + pixels(along * unit.y + across * unit.x)};
  }

  MapPoint hitch(MapPoint toward) const {
    auto delta = toward - at;
    return position(delta.x * unit.x + delta.y * unit.y < 0 ? -length.rear : length.front);
  }
};

MapPoint unitVector(MapPoint vector) {
  const int x = std::abs(vector.x), y = std::abs(vector.y);
  const int length = std::max(x, y) * 8 + std::min(x, y) * 3;
  return length ? MapPoint{vector.x * 2048 / length, vector.y * 2048 / length} : MapPoint{256, 0};
}

void vehicle(Arduino_GFX &gfx, Pose pose, Index cargo, uint16_t accent, bool mine = false) {
  const bool engine = cargo == None;
  const uint16_t color = engine ? accent : CargoColors[cargo];
  const uint16_t fill = engine ? color : White;
  const uint16_t outline = mine ? Ink : color;
  const auto a = pose.position(-pose.length.rear, -3), b = pose.position(pose.length.front, -3);
  const auto c = pose.position(pose.length.front, 3), d = pose.position(-pose.length.rear, 3);
  gfx.fillTriangle(a.x, a.y, b.x, b.y, c.x, c.y, fill);
  gfx.fillTriangle(a.x, a.y, c.x, c.y, d.x, d.y, fill);
  for (const auto &edge : {std::pair{a, b}, std::pair{b, c}, std::pair{c, d}, std::pair{d, a}})
    stroke(gfx, edge.first, edge.second, outline, false);

  if (engine) {
    const auto cab = pose.position(pose.length.front - 3);
    gfx.fillCircle(cab.x, cab.y, 1, contrastInk(accent));
    if (mine) {
      const auto marker = pose.position(0, -6);
      gfx.drawPixel(marker.x, marker.y, Ink);
      gfx.drawPixel(marker.x + 1, marker.y, Ink);
    }
  } else {
    cargoMark(gfx, pose.at, cargo, color);
  }
}

} // namespace

void drawRangierwerkIcon(Arduino_GFX &gfx, int x, int y, uint16_t accent) {
  vehicle(gfx, {{x, y}}, None, accent, true);
}

void Rangierwerk::draw(Arduino_GFX &gfx, uint16_t accent) const {
  const uint32_t now = millis();
  gfx.fillRect(Diagram::Left, 0, Diagram::Width, 142, Paper);
  if (screen == Screen::Setup || !playing()) {
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
          gfx.fillRoundRect(124, y - 3, 261, 14, 3, FocusWash);
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

  for (Index n = 0; n < layout.count; ++n) {
    const auto &node = layout.nodes[n];
    for (Index next : node.links)
      if (next < n)
        track(gfx, diagram, n, next);
  }
  for (Index n = 0; n < layout.count; ++n) {
    const auto &node = layout.nodes[n];
    if (node.links[1] == None)
      terminal(gfx, diagram.at(n), node.kind, node.cargo, node.kind == Track::Target && goal == node.cargo);
  }
  for (Index i = 0; i < layout.switchCount; ++i) {
    const Index at = layout.switches[i];
    const auto &node = layout.nodes[at];
    const bool diverging = (thrown >> i) & 1;
    const auto center = diagram.at(at);
    const bool selected = i == turnout;
    if (selected || diverging) {
      const auto stem = diagram.along(at, node.links[0], 100);
      const auto exit = diagram.along(at, node.links[diverging ? 2 : 1], 135);
      const uint16_t color = selected ? Focus : Active;
      stroke(gfx, stem, center, color, selected);
      stroke(gfx, center, exit, color, selected);
    }
    if (selected) {
      gfx.drawCircle(center.x, center.y, 5, Focus);
      gfx.fillCircle(center.x, center.y, 2, White);
    } else {
      gfx.fillCircle(center.x, center.y, 1, Active);
    }
    if ((switched & (1u << i)) && now - switchedAt < 220)
      gfx.drawCircle(center.x, center.y, 4 + (now - switchedAt) / 70, Focus);
  }

  Pose poses[MaxVehicles]{};
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto &sprite = animation[i];
    const int progress = std::min<uint32_t>(180, now - sprite.movedAt) * Diagram::Substeps / 180;
    MapPoint center = diagram.at(v.node), direction{1, 0};
    const bool moving = sprite.id == v.id && sprite.at == v.node && sprite.from != v.node &&
                        sprite.from < layout.count && progress < Diagram::Substeps;
    if (moving) {
      const int lo = std::max(0, progress - 32), hi = std::min(Diagram::Substeps, progress + 32);
      center = diagram.along(sprite.from, v.node, progress);
      direction = diagram.along(sprite.from, v.node, hi) - diagram.along(sprite.from, v.node, lo);
      if (i < Engines && v.anchorDirection > 0)
        direction = {-direction.x, -direction.y};
    } else {
      Index nose = v.anchor;
      bool invert = i < Engines && v.anchorDirection < 0;
      if (i < Engines && invert) {
        const Index front = layout.route(v.node, v.anchor, thrown);
        if (front != None) {
          nose = front;
          invert = false;
        }
      } else if (i >= Engines) {
        for (Index neighbor : v.links)
          if (neighbor < MaxVehicles && fleet[neighbor].id && fleet[neighbor].node < layout.count)
            nose = fleet[neighbor].node;
      }
      if (layout.adjacent(v.node, nose)) {
        direction = diagram.along(v.node, nose, 96) - center;
        if (invert)
          direction = {-direction.x, -direction.y};
      }
    }
    auto length = diagram.vehicleLength(layout, v.node, direction);
    if (moving) {
      const auto old = diagram.vehicleLength(layout, sprite.from, direction);
      length.rear = (old.rear * (Diagram::Substeps - progress) + length.rear * progress) / Diagram::Substeps;
      length.front = (old.front * (Diagram::Substeps - progress) + length.front * progress) / Diagram::Substeps;
    }
    poses[i] = {center, unitVector(direction), length};
  }
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto &pose = poses[i];
    for (Index next : v.links)
      if (next < i && fleet[next].id && fleet[next].node < layout.count) {
        const auto &other = poses[next];
        stroke(gfx, pose.hitch(other.at), other.hitch(pose.at),
               now - animation[i].linkAt < 300 ? Focus : Ink, false);
      }
  }
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = fleet[i];
    if (!v.id || v.node >= layout.count)
      continue;
    const auto &pose = poses[i];
    const auto [x, y] = pose.at;
    const uint16_t color = onlineMode && i < Engines ? remote.colors[i] : accent;
    if (v.unloading || (i >= Engines && now - animation[i].bornAt < 300)) {
      const int radius = v.unloading ? 5 + (UnloadMs - v.unloading) / 120 : 7;
      gfx.drawCircle(x, y, radius, Focus);
    }
    vehicle(gfx, pose, v.cargo, color, i == self());
    if (onlineMode && i < Engines) {
      const int marker = std::max(1, y - 12);
      gfx.fillRoundRect(x + 7, marker - 1, 8, 10, 2, Ink);
      const char number[]{static_cast<char>('1' + i), 0};
      text(gfx, x + 8, marker, number, (remote.connected & (1u << i)) ? White : Rail);
    }
  }

  gfx.fillRect(Diagram::Left, Diagram::Bottom, Diagram::Width, 1, Divider);
  gfx.setTextSize(1);
  gfx.setTextColor(Ink);
  if (focused) {
    cargoMark(gfx, {99, 133}, goal, CargoColors[goal]);
    gfx.setCursor(109, 130);
    gfx.printf("%u/%u  > ", unsigned(focused->delivered), unsigned(focused->count));
    printDisplayText(gfx, Destinations[goal]);
    if (count > 1) {
      gfx.setCursor(224, 130);
      gfx.setTextColor(Muted);
      gfx.printf("%u/%u", unsigned(order % count + 1), unsigned(count));
    }
  }
  gfx.setTextColor(deliveryAt && now - deliveryAt < 750 ? Focus : Ink);
  gfx.setCursor(263, 130);
  gfx.printf("Geliefert %lu", static_cast<unsigned long>(delivered()));
  text(gfx, 337, 130, "L halten", Muted);
  const auto remaining = (onlineMode ? remote.remaining : local.remaining) / 1000;
  gfx.setCursor(394, 130);
  gfx.setTextColor(Ink);
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
      vehicle(gfx, {{169, y}}, None, remote.colors[i], i == self());
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
        gfx.fillRoundRect(169, 35 + 17 * i, 179, 15, 3, FocusWash);
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
      vehicle(gfx, {{x, 66}}, fleet[id].cargo, accent, id == self());
    }
    text(gfx, 140, 85, train.size > 1 ? "R <> wählen   Mitte lösen   v zurück" :
                                                 "Kein Wagen   R unten: zurück");
  } else if (now - openedAt < 2300) {
    panel(gfx, 161, 54, 195, 17);
    text(gfx, 173, 59, "Weiche am Lokschuppen stellen");
  }
}
} // namespace leap
