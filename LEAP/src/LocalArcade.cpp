#include "Games.h"
#include "AccentColor.h"
#include "Audio.h"
#include "DisplayText.h"
#include <cstdio>
#include <cstring>

namespace leap {
namespace {
constexpr uint16_t Field = 0x18c3, Panel = 0x10e5, Covered = 0x3189;
constexpr uint16_t Ink = 0xffff, Muted = 0x9d35;
constexpr uint16_t Gold = 0xfe68, Red = 0xfb4d;

void label(Arduino_GFX &gfx, int x, int y, const char *text, uint16_t color = Ink) {
  gfx.setTextColor(color);
  gfx.setCursor(x, y);
  printDisplayText(gfx, text);
}
} // namespace

void Games::arcadeInput(const InputEvent &event) {
  if (!opened || event.longPress)
    return;
  if (isMinesweeper()) {
    if (!event.right) {
      if (event.key == Key::Up && !event.repeated && minesweeper.flag())
        audio.tone(550, 40);
      return;
    }
    if (event.key != Key::Center) {
      int dx = (event.key == Key::Right) - (event.key == Key::Left);
      int dy = (event.key == Key::Down) - (event.key == Key::Up);
      minesweeper.move(dx, dy);
    } else if (minesweeper.confirm(esp_random())) {
      auto phase = minesweeper.phase();
      audio.tone(phase == Minesweeper::Phase::Lost ? 220 :
                 phase == Minesweeper::Phase::Won ? 1047 : 660, 65);
    }
  } else if (isBreakout() && event.right && event.key == Key::Center) {
    breakout.confirm(millis());
    audio.tone(660, 55);
  }
}

const char *Games::arcadeHint() const {
  if (isMinesweeper()) {
    switch (minesweeper.phase()) {
    case Minesweeper::Phase::Choose: return "R: Stufe / Start   L Mitte: zurück";
    case Minesweeper::Phase::Playing: return "R: Feld / öffnen   L hoch: Fahne   L Mitte: zurück";
    default: return "R Mitte: nochmal   L Mitte: zurück";
    }
  }
  switch (breakout.phase()) {
  case Breakout::Phase::Ready: return "R: Schläger   R Mitte: Start   L Mitte: zurück";
  case Breakout::Phase::Playing: return "R: Schläger   R Mitte: Pause   L Mitte: zurück";
  case Breakout::Phase::Paused: return "R Mitte: weiter   L Mitte: zurück";
  default: return "R Mitte: nochmal   L Mitte: zurück";
  }
}

void Games::drawMinesweeper(Arduino_GFX &gfx, int left, int top, uint16_t accent) const {
  using Phase = Minesweeper::Phase;
  using Cover = Minesweeper::Cover;
  auto phase = minesweeper.phase();
  gfx.setTextSize(1);
  label(gfx, left, top, "Minesweeper", accent);
  char line[48];
  if (phase == Phase::Choose) {
    label(gfx, left, top + 18, "Schwierigkeit wählen");
    for (int i = 0; i < Minesweeper::LevelCount; ++i) {
      int x = left + i * 109;
      bool selected = i == minesweeper.difficulty();
      const auto &level = Minesweeper::Levels[i];
      uint16_t text = selected ? contrastInk(accent) : Ink;
      gfx.fillRoundRect(x, top + 37, 104, 37, 4, selected ? accent : Panel);
      label(gfx, x + 7, top + 44, level.name, text);
      std::snprintf(line, sizeof(line), "%dx%d / %d Minen", level.columns, level.rows, level.mines);
      label(gfx, x + 7, top + 58, line, selected ? text : Muted);
    }
    return;
  }

  constexpr int tile = 12;
  constexpr uint16_t numbers[] = {Muted, 0x6dff, 0x6ff0, Gold, Red, Ink, 0x6dff, Gold, Red};
  auto &level = minesweeper.level();
  int gridX = left + (240 - level.columns * tile) / 2;
  int gridY = top + 16 + (84 - level.rows * tile) / 2;
  for (int row = 0; row < level.rows; ++row) {
    for (int col = 0; col < level.columns; ++col) {
      const auto &cell = minesweeper.cell(col, row);
      int x = gridX + col * tile, y = gridY + row * tile;
      bool exploded = cell.mine && cell.cover == Cover::Open;
      bool shownMine = cell.mine && phase == Phase::Lost && cell.cover != Cover::Flagged;
      gfx.fillRect(x, y, 11, 11, cell.cover == Cover::Open || shownMine ? Panel : Covered);
      if (shownMine)
        gfx.fillCircle(x + 5, y + 5, exploded ? 3 : 2, Red);
      else if (cell.cover == Cover::Flagged) {
        bool wrong = phase == Phase::Lost && !cell.mine;
        label(gfx, x + 3, y + 2, wrong ? "X" : "F", wrong ? Red : Gold);
      } else if (cell.cover == Cover::Open && cell.neighbors) {
        char digit[] = {char('0' + cell.neighbors), '\0'};
        label(gfx, x + 3, y + 2, digit, numbers[cell.neighbors]);
      }
      if (col == minesweeper.cursorX() && row == minesweeper.cursorY())
        gfx.drawRect(x, y, 11, 11, accent);
    }
  }
  int info = left + 245;
  gfx.fillRoundRect(info, top + 16, 77, 84, 4, Panel);
  label(gfx, info + 5, top + 23, level.name, accent);
  std::snprintf(line, sizeof(line), "Minen %d", level.mines);
  label(gfx, info + 5, top + 40, line);
  std::snprintf(line, sizeof(line), "Fahnen %d", minesweeper.flagged());
  label(gfx, info + 5, top + 54, line);
  if (phase == Phase::Won || phase == Phase::Lost)
    label(gfx, info + 5, top + 76, phase == Phase::Won ? "Gewonnen!" : "Mine!", phase == Phase::Won ? accent : Red);
}

void Games::drawBreakout(Arduino_GFX &gfx, int left, int top, uint16_t accent) const {
  using B = Breakout;
  constexpr uint16_t colors[B::Rows] = {Red, Gold, 0x5e8d, 0x5c9f};
  gfx.setTextSize(1);
  label(gfx, left, top, "Breakout", accent);
  gfx.setTextColor(Ink);
  gfx.setCursor(left + 83, top);
  gfx.printf("Level %d/%d    Punkte %d    Leben %d", breakout.level(), B::LevelCount,
             breakout.score(), breakout.lives());

  int y = top + 16;
  gfx.fillRoundRect(left, y, B::Width, B::Height, 4, Panel);
  for (int row = 0; row < B::Rows; ++row)
    for (int col = 0; col < B::Columns; ++col)
      if (breakout.brick(row, col))
        gfx.fillRoundRect(left + B::BrickX + col * B::BrickPitchX, y + B::BrickY + row * B::BrickPitchY,
                          B::BrickW, B::BrickH, 2, colors[row]);
  gfx.fillRoundRect(left + breakout.paddleX() - B::PaddleHalf, y + B::PaddleY,
                    B::PaddleHalf * 2, B::PaddleH, 2, accent);
  if (breakout.phase() != B::Phase::Lost && breakout.ballY() + B::BallRadius < B::Height)
    gfx.fillCircle(left + breakout.ballX(), y + breakout.ballY(), B::BallRadius, Ink);

  const char *status = nullptr;
  switch (breakout.phase()) {
  case B::Phase::Ready: status = "Ball bereit"; break;
  case B::Phase::Paused: status = "Pause"; break;
  case B::Phase::Won: status = "Geschafft!"; break;
  case B::Phase::Lost: status = "Spielende"; break;
  case B::Phase::Playing: break;
  }
  if (status) {
    gfx.fillRoundRect(left + (B::Width - 106) / 2, y + 46, 106, 20, 4, Field);
    label(gfx, left + (B::Width - 6 * int(std::strlen(status))) / 2, y + 52, status, accent);
  }
}
} // namespace leap
