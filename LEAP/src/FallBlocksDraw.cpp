#include "Games.h"
#include "DisplayText.h"

namespace leap {
namespace {
constexpr int BoardX = 11, BoardY = 160, CellSize = 12;
constexpr int BoardWidth = FallBlocks::Columns * CellSize, BoardHeight = FallBlocks::Rows * CellSize;
constexpr uint16_t BoardColor = 0x0862, GridColor = 0x19a8, GhostColor = 0x6b8e;
constexpr uint16_t Ink = 0xffff, Muted = 0xb5d8, Overlay = 0x18e7;
constexpr uint16_t Colors[] = {0, 0x4edd, 0xfe24, 0xb29e, 0xa6cb, 0xfa92, 0x643e, 0xfc0b};

void label(Arduino_GFX &gfx, int x, int y, const char *text, uint16_t color, int size = 1) {
  gfx.setCursor(x, y);
  gfx.setTextColor(color);
  gfx.setTextSize(size);
  printDisplayText(gfx, text);
}

void number(Arduino_GFX &gfx, int x, int y, int value) {
  gfx.setCursor(x, y);
  gfx.setTextColor(Ink);
  gfx.setTextSize(1);
  gfx.printf("%d", value);
}

void tile(Arduino_GFX &gfx, int x, int y, int size, FallBlocks::Shape shape) {
  uint16_t color = Colors[shape];
  gfx.fillRect(x + 1, y + 1, size - 1, size - 1, color);
  gfx.drawFastHLine(x + 2, y + 2, size - 3, Ink);
}
} // namespace

void Games::drawFallBlocks(Arduino_GFX &gfx, uint16_t accent) {
  gfx.setFont();
  label(gfx, 11, 115, "Punkte", Muted);
  number(gfx, 11, 125, fallBlocks.score());
  label(gfx, 11, 138, "Rekord", Muted);
  number(gfx, 11, 148, fallBlocksBest);
  label(gfx, 83, 115, "Nächster", Muted);
  if (auto next = fallBlocks.next(); next != FallBlocks::Empty) {
    constexpr int PreviewCell = 8;
    int left = 107 - (next == FallBlocks::Bar ? 16 : 12);
    for (int i = 0; i < 4; ++i) {
      auto block = FallBlocks::block(next, 0, i);
      tile(gfx, left + block.x * PreviewCell, 131 + block.y * PreviewCell, PreviewCell, next);
    }
  }

  gfx.fillRect(BoardX, BoardY, BoardWidth, BoardHeight, BoardColor);
  gfx.drawRect(BoardX - 1, BoardY - 1, BoardWidth + 2, BoardHeight + 2, accent);
  for (int x = 0; x < FallBlocks::Columns; ++x)
    gfx.fillRect(BoardX + x * CellSize, BoardY, 1, BoardHeight, GridColor);
  for (int y = 0; y < FallBlocks::Rows; ++y) {
    gfx.drawFastHLine(BoardX, BoardY + y * CellSize, BoardWidth, GridColor);
    for (int x = 0; x < FallBlocks::Columns; ++x)
      if (auto shape = fallBlocks.cell(x, y); shape != FallBlocks::Empty)
        tile(gfx, BoardX + x * CellSize, BoardY + y * CellSize, CellSize, shape);
  }

  switch (fallBlocks.phase()) {
    case FallBlocks::Phase::Playing: {
      const auto &piece = fallBlocks.piece();
      int landing = fallBlocks.landingRow();
      for (int i = 0; i < 4; ++i) {
        auto block = FallBlocks::block(piece.shape, piece.turns, i);
        gfx.drawRect(BoardX + (piece.x + block.x) * CellSize + 2,
                     BoardY + (landing + block.y) * CellSize + 2,
                     CellSize - 3, CellSize - 3, GhostColor);
      }
      for (int i = 0; i < 4; ++i) {
        auto block = FallBlocks::block(piece.shape, piece.turns, i);
        tile(gfx, BoardX + (piece.x + block.x) * CellSize, BoardY + (piece.y + block.y) * CellSize,
             CellSize, piece.shape);
      }
      break;
    }
    case FallBlocks::Phase::Ready:
      gfx.fillRect(12, 248, 118, 56, Overlay);
      label(gfx, 17, 253, "Fallblöcke", Ink);
      label(gfx, 17, 269, "Gerät hochkant", Muted);
      label(gfx, 17, 285, "Unten Mitte: Start", accent);
      break;
    case FallBlocks::Phase::Over:
      gfx.fillRect(12, 248, 118, 46, Overlay);
      label(gfx, 17, 255, "Spielende", Ink);
      label(gfx, 17, 274, "Unten Mitte: Neu", accent);
      break;
  }
  label(gfx, 11, 405, "Stufe", Muted);
  number(gfx, 47, 405, fallBlocks.level());
  label(gfx, 11, 417, "Obere Mitte: Zurück", Muted);
}
} // namespace leap
