#include "DiceGame.h"
#include "AccentColor.h"
#include "DiceReel.h"
#include "DisplayText.h"
#include <Arduino_GFX_Library.h>
#include <cstdio>

namespace leap {
namespace {
enum class Face { Pips, Number, Lotti, Coin, Color };
struct Option {
  const char *label;
  uint8_t faces;
  Face style;
};
constexpr Option Options[] = {
    {"W6", 6, Face::Pips},             {"W3 (1 bis 3)", 3, Face::Pips},
    {"Lotti Karotti", 4, Face::Lotti}, {"Kopf oder Zahl", 2, Face::Coin},
    {"Farbwürfel", 6, Face::Color},   {"W4", 4, Face::Number},
    {"W8", 8, Face::Number},          {"W10", 10, Face::Number},
    {"W12", 12, Face::Number},        {"W20", 20, Face::Number},
};
constexpr int Standard = 0, Columns = 2, OptionCount = sizeof(Options) / sizeof(Options[0]);
constexpr uint32_t SpinMs = 360, FirstBrakeMs = 440, BrakeStepMs = 155;
constexpr int Pitch = 46, DieSize = 52, Gap = 12;
constexpr uint16_t Ink = 0x1084, Light = 0xffdf, Muted = 0xbdf7, Panel = 0x212a;
constexpr uint16_t HeldRim = 0x5e8a, Orange = 0xfe00, Green = 0x44e4;

struct ColorFace {
  const char *name;
  uint16_t color;
};
constexpr ColorFace Colors[] = {{"Rot", 0xe986}, {"Orange", 0xfca0}, {"Gelb", 0xff24},
                                {"Grün", 0x3e69}, {"Blau", 0x44dd}, {"Lila", 0xa35c}};
constexpr const char *CoinFaces[] = {"Kopf", "Zahl"};

uint8_t randomFace(const Option &option) {
  if (option.style != Face::Lotti)
    return randomBelow(esp_random, option.faces);
  // The 48-card set has 24 singles, 8 doubles, 4 triples and 12 carrots.
  const auto card = randomBelow(esp_random, 48);
  return card < 24 ? 0 : card < 32 ? 1 : card < 36 ? 2 : 3;
}

void text(Arduino_GFX &gfx, int x, int y, const char *value, uint16_t color = Light,
          int size = 1, bool centered = false) {
  auto glyphs = displayGlyphs(value);
  gfx.setTextColor(color);
  gfx.setTextSize(size);
  if (centered)
    x -= int(glyphs.size()) * 3 * size;
  gfx.setCursor(x, y);
  gfx.print(glyphs.c_str());
}

void footer(Arduino_GFX &gfx, int left, const char *hint) {
  gfx.fillRect(left - 8, 126, hw::Width - left + 8, hw::Height - 126, Panel);
  text(gfx, left, 130, hint, Light);
  text(gfx, hw::Width - 44, 130, "L OK: zurück", Muted, 1, true);
}

void optionTile(Arduino_GFX &gfx, int x, int y, int w, int h, const char *label,
                bool selected, uint16_t accent) {
  if (selected)
    gfx.fillRoundRect(x, y, w, h, 4, Light);
  const uint16_t background = selected ? accent : Panel;
  gfx.fillRoundRect(x + 1, y + 1, w - 2, h - 2, 3, background);
  text(gfx, x + w / 2, y + (h - 8) / 2, label,
       selected ? contrastInk(background) : Light, 1, true);
}

void pips(Arduino_GFX &gfx, int x, int y, uint8_t count, int top, int bottom) {
  constexpr uint16_t masks[] = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16d};
  for (int pos = 0; pos < 9; ++pos) {
    const int py = y + (pos / 3 - 1) * 11;
    if ((masks[count - 1] & (1u << pos)) && py - 3 >= top && py + 3 < bottom)
      gfx.fillCircle(x + (pos % 3 - 1) * 11, py, 3, Ink);
  }
}

void faceChip(Arduino_GFX &gfx, const Option &option, int x, int y, uint8_t face, int top, int bottom) {
  const int chipTop = std::max(top, y - Pitch / 2 + 1);
  const int chipBottom = std::min(bottom, y + Pitch / 2 - 1);
  if (chipTop >= chipBottom)
    return;
  const uint16_t fill = option.style == Face::Color ? Colors[face].color : Light;
  gfx.fillRect(x - 22, chipTop, 44, chipBottom - chipTop, fill);
  if (option.style == Face::Pips || (option.style == Face::Lotti && face < 3)) {
    pips(gfx, x, y, face + 1, top, bottom);
  } else if (option.style == Face::Lotti && y - 14 >= top && y + 12 < bottom) {
    gfx.fillTriangle(x - 8, y - 6, x + 7, y - 1, x - 10, y + 12, Orange);
    gfx.drawLine(x, y - 4, x - 2, y - 13, Green);
    gfx.drawLine(x, y - 4, x + 5, y - 14, Green);
    gfx.drawLine(x, y - 4, x + 10, y - 9, Green);
  } else if (option.style == Face::Coin && y - 14 >= top && y + 14 < bottom) {
    gfx.fillCircle(x, y, 14, Orange);
    gfx.drawCircle(x, y, 12, Ink);
    text(gfx, x, y - 4, face == 0 ? "K" : "Z", Ink, 1, true);
  } else if (option.style == Face::Number && y - 8 >= top && y + 8 <= bottom) {
    char number[4];
    std::snprintf(number, sizeof(number), "%u", unsigned(face + 1));
    text(gfx, x, y - 8, number, Ink, 2, true);
  }
}

void die(Arduino_GFX &gfx, const Option &option, int x, int y, int face,
         bool held, bool selected, bool spinning, uint32_t scroll) {
  if (selected)
    gfx.fillRoundRect(x - 3, y - 3, DieSize + 6, DieSize + 6, 9, Light);
  gfx.fillRoundRect(x, y, DieSize, DieSize, 6, held ? HeldRim : Panel);
  const int center = x + DieSize / 2, middle = y + DieSize / 2;
  const int top = y + 4, bottom = y + DieSize - 4;
  if (spinning) {
    const int index = int((scroll / Pitch) % option.faces);
    const int phase = int(scroll % Pitch);
    for (int offset = -1; offset <= 1; ++offset) {
      const uint8_t side = (2 * option.faces + offset - index) % option.faces;
      faceChip(gfx, option, center, middle + phase + offset * Pitch, side, top, bottom);
    }
  } else if (face >= 0) {
    faceChip(gfx, option, center, middle, face, top, bottom);
  } else {
    text(gfx, center, middle - 8, "?", Muted, 2, true);
  }
  if (held)
    text(gfx, center, y + DieSize + 6, "Fest", HeldRim, 1, true);
}
} // namespace

int DiceGame::diceCount() const { return type == Standard ? count : 1; }

void DiceGame::play() {
  screen = Screen::Play;
  selected = 0;
  dice = {};
  hasResult = false;
  rollDuration = 0;
}

void DiceGame::roll(uint32_t now) {
  uint32_t brakeMs = FirstBrakeMs;
  for (int i = 0; i < diceCount(); ++i) {
    auto &die = dice[i];
    die.brakeMs = 0;
    if (die.held)
      continue;
    die.face = randomFace(Options[type]);
    die.brakeMs = brakeMs;
    brakeMs += BrakeStepMs;
  }
  if (brakeMs == FirstBrakeMs)
    return;
  hasResult = true;
  rolledAt = now;
  rollDuration = SpinMs + brakeMs - BrakeStepMs;
}

bool DiceGame::input(const InputEvent &event, uint32_t now) {
  if (event.longPress)
    return false;
  const Key key = event.key;
  if (!event.right) {
    if (key != Key::Center)
      return false;
    if (screen == Screen::Types)
      return true;
    screen = Screen::Types;
    rollDuration = 0;
    return false;
  }
  if (screen == Screen::Types) {
    if (key == Key::Center) {
      if (type == Standard)
        screen = Screen::Count;
      else
        play();
    } else if (key == Key::Up || key == Key::Down) {
      type = std::clamp(type + (key == Key::Down ? Columns : -Columns), type % Columns,
                        OptionCount - Columns + type % Columns);
    } else {
      type = (type / Columns) * Columns + (key == Key::Right ? 1 : 0);
    }
  } else if (screen == Screen::Count) {
    const int delta = (key == Key::Right || key == Key::Down) - (key == Key::Left || key == Key::Up);
    count = std::clamp(count + delta, 1, MaxDice);
    if (key == Key::Center)
      play();
  } else {
    tick(now);
    if (animating())
      return false;
    if (key == Key::Center)
      roll(now);
    else if (diceCount() >= 2 && hasResult) {
      if (key == Key::Left || key == Key::Right)
        selected = std::clamp(selected + (key == Key::Right ? 1 : -1), 0, count - 1);
      if (key == Key::Up && !event.repeated)
        dice[selected].held = !dice[selected].held;
    }
  }
  return false;
}

void DiceGame::tick(uint32_t now) {
  if (animating() && elapsed(now, rolledAt, rollDuration))
    rollDuration = 0;
}

void DiceGame::draw(Arduino_GFX &gfx, int left, uint16_t accent, uint32_t now) const {
  gfx.setFont(nullptr);
  const int width = hw::Width - left - 8, center = left + width / 2;
  text(gfx, left, 8, "Würfeln", Light, 2);
  if (screen == Screen::Types) {
    const int tileWidth = (width - 8) / Columns;
    for (int i = 0; i < OptionCount; ++i)
      optionTile(gfx, left + (i % Columns) * (tileWidth + 8), 28 + (i / Columns) * 18,
                 tileWidth, 16, Options[i].label, i == type, accent);
    footer(gfx, left, "R: Richtung wählen, OK bestätigen");
    return;
  }
  if (screen == Screen::Count) {
    text(gfx, center, 39, "Wie viele W6?", Light, 2, true);
    constexpr int Tile = 44;
    for (int i = 0; i < MaxDice; ++i) {
      const char label[] = {char('1' + i), '\0'};
      optionTile(gfx, center - (MaxDice * Tile + (MaxDice - 1) * Gap) / 2 + i * (Tile + Gap),
                 66, Tile, 30, label, i + 1 == count, accent);
    }
    footer(gfx, left, "R: Anzahl wählen, OK bestätigen");
    return;
  }

  const auto &option = Options[type];
  const int n = diceCount();
  char status[24];
  if (type == Standard)
    std::snprintf(status, sizeof(status), "%d x W6", n);
  text(gfx, left + 226, 12, type == Standard ? status : option.label, Muted);
  const int start = center - (n * DieSize + (n - 1) * Gap) / 2;
  const uint32_t age = now - rolledAt;
  int total = 0, held = 0;
  for (int i = 0; i < n; ++i) {
    const auto &state = dice[i];
    const bool spinning = animating() && state.brakeMs && age < SpinMs + state.brakeMs;
    uint32_t scroll = 0;
    if (spinning) {
      const uint32_t offset = 1 + i * 31;
      scroll = age < SpinMs ? offset + age / 4 :
               brakeReel(age - SpinMs, state.brakeMs, offset + SpinMs / 4, Pitch, option.faces, state.face);
    }
    die(gfx, option, start + i * (DieSize + Gap), 33, hasResult ? state.face : -1, state.held,
        n >= 2 && hasResult && i == selected, spinning, scroll);
    total += state.face + 1;
    held += state.held;
  }

  const char *message = "";
  if (animating()) {
    message = n == 1 ? "Der Würfel rollt ..." : "Die Würfel rollen ...";
  } else if (hasResult) {
    if (n >= 2) {
      std::snprintf(status, sizeof(status), "Summe: %d", total);
      message = status;
    } else if (option.style == Face::Lotti && dice[0].face == 3) {
      message = "Karotte";
    } else if (option.style == Face::Coin) {
      message = CoinFaces[dice[0].face];
    } else if (option.style == Face::Color) {
      message = Colors[dice[0].face].name;
    }
  }
  text(gfx, center, 105, message, Light, animating() ? 1 : 2, true);
  footer(gfx, left, held == n ? "Alle fest  |  R Hoch: lösen" :
                    n >= 2 && hasResult && !animating() ? "R: OK Wurf, L/R Wahl, Hoch halten/lösen" :
                    animating() ? "" : "R OK: würfeln");
}
} // namespace leap
