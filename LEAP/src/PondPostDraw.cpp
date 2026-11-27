#include "PondPost.h"
#include "AccentColor.h"
#include "DisplayText.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace leap {
namespace {
constexpr uint16_t rgb(int r, int g, int b) { return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); }
constexpr uint16_t Ink = rgb(17, 38, 38), DarkWater = rgb(21, 73, 83), Water = rgb(38, 112, 115);
constexpr uint16_t Ripple = rgb(101, 178, 175), Light = rgb(255, 247, 209), Gold = rgb(252, 204, 106);
constexpr uint16_t Grass = rgb(75, 110, 73), Pad = rgb(88, 141, 91), PadLight = rgb(132, 179, 110);
constexpr int Left = 86, Width = 342, FirstPad = 150, PadSpacing = 18, PostX = 402;
constexpr uint32_t LeftBrakeMs = 520, RightBrakeMs = 780, HopMs = 175, ReelPitch = 19;

void text(Arduino_GFX &gfx, int x, int y, const char *value, uint16_t color = Light) {
  gfx.setCursor(x, y);
  gfx.setTextColor(color);
  printDisplayText(gfx, value);
}

void mark(Arduino_GFX &gfx, uint8_t lane, int x, int y, uint16_t color) {
  switch (lane) {
  case 0: gfx.fillRect(x - 1, y - 1, 3, 3, color); break;
  case 1: gfx.fillTriangle(x, y - 2, x - 2, y + 1, x + 2, y + 1, color); break;
  case 2:
    gfx.drawFastHLine(x - 2, y, 5, color);
    gfx.fillRect(x, y - 2, 1, 5, color);
    break;
  case 3:
    gfx.drawFastHLine(x - 1, y - 1, 3, color);
    gfx.drawFastHLine(x - 1, y + 1, 3, color);
    break;
  }
}

void frog(Arduino_GFX &gfx, int x, int y, uint8_t lane, uint16_t color) {
  // Pale rim keeps even a dark device accent visible against the pond.
  gfx.fillRoundRect(x - 9, y - 5, 19, 12, 4, Ink);
  gfx.fillRoundRect(x - 8, y - 4, 17, 10, 3, Light);
  gfx.fillRoundRect(x - 7, y - 3, 15, 8, 3, color);
  gfx.fillCircle(x - 4, y - 5, 4, Ink);
  gfx.fillCircle(x + 4, y - 5, 4, Ink);
  gfx.fillCircle(x - 4, y - 5, 3, Light);
  gfx.fillCircle(x + 4, y - 5, 3, Light);
  gfx.fillRect(x - 4, y - 6, 2, 3, Ink);
  gfx.fillRect(x + 4, y - 6, 2, 3, Ink);
  gfx.drawFastHLine(x - 7, y + 6, 4, color);
  gfx.drawFastHLine(x + 4, y + 6, 4, color);
  mark(gfx, lane, x, y + 2, contrastInk(color));
}

void envelope(Arduino_GFX &gfx, int x, int y, uint16_t paper) {
  gfx.fillRoundRect(x - 7, y - 5, 15, 10, 2, Ink);
  gfx.fillRect(x - 6, y - 4, 13, 8, paper);
  gfx.drawLine(x - 5, y - 3, x, y, Ink);
  gfx.drawLine(x, y, x + 5, y - 3, Ink);
}

void scenery(Arduino_GFX &gfx) {
  gfx.fillRect(Left, 0, Width, 142, DarkWater);
  gfx.fillRect(Left, 16, Width, 96, Water);
  gfx.fillRoundRect(89, 17, 53, 93, 7, Grass);
  gfx.fillRoundRect(385, 17, 41, 93, 7, Grass);
  for (int row = 0; row < PondLanes; ++row) {
    int y = 29 + row * 23;
    for (int col = 0; col <= PondGoal; ++col) {
      const int x = FirstPad + col * PadSpacing;
      gfx.drawFastHLine(x - 6, y + 6, 13, DarkWater);
      gfx.fillRoundRect(x - 6, y + 1, 13, 5, 2, col == PondGoal ? Gold : Pad);
      gfx.drawFastHLine(x - 3, y + 2, 7, col == PondGoal ? Light : PadLight);
      if ((row + col) % 3 == 1)
        gfx.drawFastHLine(x + 3, y - 7, 5, Ripple);
    }
    for (int x : {136, 417}) {
      gfx.drawLine(x, y + 9, x - 2, y + 3, PadLight);
      gfx.drawLine(x, y + 9, x + 2, y + 5, PadLight);
    }
  }
  for (int x : {151, 208, 265, 322, 373}) {
    gfx.drawFastHLine(x, 107, 8, Ripple);
    gfx.drawFastHLine(x + 4, 109, 5, DarkWater);
  }
}

void dieChip(Arduino_GFX &gfx, int x, int center, uint8_t lane, uint16_t color) {
  const int y0 = std::max(center - 8, 116), y1 = std::min(center + 9, 138);
  if (y0 < y1)
    gfx.fillRect(x + 4, y0, 16, y1 - y0, color);
  if (center >= 118 && center < 136)
    mark(gfx, lane, x + 12, center, contrastInk(color));
}

void die(Arduino_GFX &gfx, int x, const uint16_t *colors, uint8_t face, uint32_t scroll) {
  gfx.fillRoundRect(x, 113, 24, 27, 4, Light);
  gfx.fillRoundRect(x + 2, 115, 20, 23, 3, Ink);
  if (face < PondLanes) {
    dieChip(gfx, x, 127, face, colors[face]);
  } else if (scroll) {
    const unsigned index = scroll / ReelPitch;
    const int y = 127 + int(scroll % ReelPitch);
    for (int offset = -1; offset <= 1; ++offset) {
      const uint8_t lane = (offset - index) & (PondLanes - 1);
      dieChip(gfx, x, y + int(ReelPitch) * offset, lane, colors[lane]);
    }
  } else {
    text(gfx, x + 9, 123, "?");
  }
}

uint32_t brakeScroll(uint32_t age, uint32_t duration, uint32_t seed, uint8_t result) {
  // Align the physical reel to the confirmed face, then ease to zero speed.
  uint32_t tile = (seed + duration / 8 + ReelPitch - 1) / ReelPitch;
  tile += (0u - result - tile) & (PondLanes - 1);
  const uint32_t distance = tile * ReelPitch - seed;
  return seed + distance * age * (2 * duration - age) / (duration * duration);
}
} // namespace

void PondPost::draw(Arduino_GFX &gfx, uint16_t accent, uint32_t now) const {
  gfx.setFont(nullptr);
  gfx.setTextSize(1);
  scenery(gfx);
  const auto &frame = state();
  const auto current = phase();
  const bool boardReady = !menu && (!onlineMode || frame.session[0]);
  uint16_t colors[PondLanes];
  uint32_t remaining = frame.remainingMs;
  if (onlineMode)
    remaining -= std::min(remaining, now - receivedAt);
  const uint32_t settling = current == PondPhase::Settling ? PondSettleMs - std::min(PondSettleMs, remaining) : 0;
  const uint32_t hopping = settling > RightBrakeMs ? settling - RightBrakeMs : 0;
  for (uint8_t lane = 0; lane < PondLanes; ++lane) {
    const auto &piece = frame.frogs[lane];
    const auto color = boardReady ? (lane == 0 && !onlineMode ? accent : piece.color) : accentColor(PondColors[lane]);
    colors[lane] = color;
    const int y = 29 + lane * 23;
    const bool selected = boardReady && (frame.turn == lane ||
                          (current == PondPhase::Finished && (frame.winners & (1 << lane))));
    if (selected)
      gfx.fillRoundRect(91, y - 8, 45, 15, 3, Light);
    mark(gfx, lane, 96, y, selected ? Ink : Light);
    auto name = displayGlyphs(boardReady ? piece.name : PondNames[lane]);
    name.resize(std::min<size_t>(name.size(), 6));
    gfx.setCursor(102, y - 3);
    gfx.setTextColor(selected ? Ink : Light);
    gfx.print(name.c_str());
    bool delivered = boardReady && (frame.winners & (1 << lane)) &&
                     (current != PondPhase::Settling || hopping >= HopMs * (frame.positions[lane] - frame.previous[lane]));
    envelope(gfx, PostX, y, delivered ? Gold : Light);
    int x = FirstPad + PadSpacing * (boardReady ? frame.positions[lane] : 0);
    int lift = 0;
    if (current == PondPhase::Settling) {
      const int steps = frame.positions[lane] - frame.previous[lane];
      const int progress = std::min<int>(hopping, steps * HopMs);
      x = FirstPad + PadSpacing * frame.previous[lane] + PadSpacing * progress / HopMs;
      const int hop = progress % HopMs;
      lift = 10 * std::min(hop, int(HopMs) - hop) / HopMs;
    }
    gfx.drawFastHLine(x - 5, y + 8, 11, DarkWater);
    frog(gfx, x, y - lift, lane, color);
  }
  if (menu) {
    gfx.fillRoundRect(165, 22, 199, 82, 7, Ink);
    text(gfx, 175, 32, "Pfützenpost", Gold);
    for (int i = 0; i < 2; ++i) {
      const bool selected = i == chooseOnline;
      if (selected)
        gfx.fillRoundRect(173, 49 + i * 24, 182, 20, 4, Light);
      text(gfx, 180, 55 + i * 24, i ? "Online zusammen spielen" : "Allein spielen", selected ? Ink : Light);
    }
    text(gfx, 94, 122, "Rechts Mitte bestätigt");
    return;
  }
  text(gfx, 94, 4, "Pfützenpost", Gold);
  if (frame.turn < PondLanes && frame.turn != frame.self) {
    auto activeName = displayGlyphs(frame.frogs[frame.turn].name);
    activeName.resize(std::min<size_t>(activeName.size(), 15));
    activeName += " am Zug";
    gfx.setCursor(206, 4);
    gfx.setTextColor(Light);
    gfx.print(activeName.c_str());
  }
  if (frame.round) {
    char round[18];
    std::snprintf(round, sizeof(round), "Runde %lu", static_cast<unsigned long>(frame.round));
    text(gfx, 348, 4, round);
  }
  const int connected = std::count_if(std::begin(frame.frogs), std::end(frame.frogs), [](const PondFrog &f) {
    return f.human && f.connected;
  });
  const bool self = frame.turn == frame.self;
  char status[48]{};
  const char *hint = "";
  switch (current) {
  case PondPhase::Connecting: hint = "Verbinde mit Homeserver ..."; break;
  case PondPhase::Lobby:
    if (connected < 2)
      hint = "Warte auf Mitspieler ...";
    else if (std::any_of(std::begin(frame.frogs), std::end(frame.frogs), [](const PondFrog &f) { return f.human && !f.connected; }))
      hint = "Warte auf WLAN ...";
    else if (frame.host == frame.self)
      hint = "Mitte startet das Rennen";
    else
      hint = "Gastgeber startet das Rennen";
    break;
  case PondPhase::Paused: hint = "Warte auf WLAN ..."; break;
  case PondPhase::Error:
    hint = !std::strcmp(frame.reason, "game_disabled") ? "Spiel im Homeserver freigeben" : "Homeserver nicht erreichbar";
    break;
  case PondPhase::Finished:
    if (!std::strcmp(frame.reason, "lobby_timeout")) hint = "Kein Mitspieler gefunden";
    else if (!std::strcmp(frame.reason, "players_left")) hint = "Partie beendet";
    else if (frame.winners & (1 << frame.self)) hint = "Du hast Post zugestellt!";
    else hint = "Die Post ist da!";
    break;
  case PondPhase::Ready: hint = self ? "Du bist dran! Mitte würfelt" : "Warte auf den Würfelwurf ..."; break;
  case PondPhase::Rolling: hint = self && !stopSent ? "Mitte stoppt die Würfel" : "Die Würfel rollen ..."; break;
  case PondPhase::Settling:
    hint = settling < RightBrakeMs ? "Die Würfel rollen aus ..." :
           frame.dice[0] == frame.dice[1] ? "Pasch! Zwei Felder" : "Die Frösche ziehen!";
    break;
  }
  if ((current == PondPhase::Lobby || current == PondPhase::Ready || current == PondPhase::Rolling ||
       current == PondPhase::Paused) && onlineMode && remaining) {
    std::snprintf(status, sizeof(status), "%lus", static_cast<unsigned long>((remaining + 999) / 1000));
    text(gfx, 395, 122, status);
  } else if (current == PondPhase::Finished || current == PondPhase::Error) {
    text(gfx, 346, 131, "Mitte: Menü");
  }
  text(gfx, 150, 122, hint);
  for (unsigned i = 0; i < 2; ++i) {
    uint8_t face = frame.dice[i];
    uint32_t scroll = 0;
    if (current == PondPhase::Rolling) {
      face = PondNone;
      scroll = 1 + (now - rollingAt) / 4 + i * 43;
    } else if (current == PondPhase::Settling) {
      const uint32_t duration = i == 0 ? LeftBrakeMs : RightBrakeMs;
      if (settling < duration && face < PondLanes) {
        scroll = brakeScroll(settling, duration, brakeStart + i * 43, face);
        face = PondNone;
      }
    }
    die(gfx, 94 + i * 27, colors, face, scroll);
  }
}
} // namespace leap
