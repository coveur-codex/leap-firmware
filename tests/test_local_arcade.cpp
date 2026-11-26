#include "Breakout.h"
#include "Minesweeper.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace leap;

static void verifyMines(int difficulty, uint32_t seed) {
  using Cover = Minesweeper::Cover;
  Minesweeper game;
  game.open();
  game.move(difficulty - game.difficulty(), 0);
  game.confirm(seed);
  const auto &level = game.level();
  int firstX = seed % level.columns, firstY = (seed / level.columns) % level.rows;
  auto select = [&](int x, int y) { game.move(x - game.cursorX(), y - game.cursorY()); };
  select(firstX, firstY);
  assert(game.flag() && game.flagged() == 1);
  assert(!game.confirm(seed) && game.cell(firstX, firstY).cover == Cover::Flagged);
  assert(game.flag() && game.flagged() == 0);
  assert(game.confirm(seed) && !game.flag());
  assert(game.cell(firstX, firstY).cover == Cover::Open);
  assert(game.cell(firstX, firstY).neighbors == 0);

  int count = 0;
  for (int y = 0; y < level.rows; ++y) {
    for (int x = 0; x < level.columns; ++x) {
      auto &cell = game.cell(x, y);
      if (cell.mine) {
        assert(std::abs(firstX - x) > 1 || std::abs(firstY - y) > 1);
        ++count;
      }
      int near = 0;
      for (int ny = std::max(0, y - 1); ny <= std::min(level.rows - 1, y + 1); ++ny)
        for (int nx = std::max(0, x - 1); nx <= std::min(level.columns - 1, x + 1); ++nx)
          if ((nx != x || ny != y) && game.cell(nx, ny).mine)
            ++near;
      assert(cell.neighbors == near);
    }
  }
  assert(count == level.mines);
  for (int y = 0; y < level.rows && game.phase() != Minesweeper::Phase::Won; ++y)
    for (int x = 0; x < level.columns && game.phase() != Minesweeper::Phase::Won; ++x)
      if (!game.cell(x, y).mine) {
        select(x, y);
        game.confirm(seed);
      }
  assert(game.phase() == Minesweeper::Phase::Won);
}

static void verifyMineLoss() {
  Minesweeper game;
  game.open();
  game.move(1, 0);
  game.confirm(1);
  game.confirm(1);
  assert(game.phase() == Minesweeper::Phase::Playing);
  for (int y = 0; y < game.level().rows; ++y)
    for (int x = 0; x < game.level().columns; ++x)
      if (game.cell(x, y).mine) {
        game.move(x - game.cursorX(), y - game.cursorY());
        game.confirm(1);
        assert(game.phase() == Minesweeper::Phase::Lost);
        game.confirm(2);
        assert(game.phase() == Minesweeper::Phase::Playing && !game.cell(x, y).mine);
        return;
      }
  assert(false && "a board has at least one mine");
}

static void verifyBreakout() {
  using Phase = Breakout::Phase;
  Breakout game;
  uint32_t now = UINT32_MAX - 24;
  game.open(now);
  auto start = game.paddleX();
  game.tick(now += 40, 1);
  assert(game.paddleX() > start && game.paddleX() == game.ballX());
  int stopped = game.paddleX();
  game.tick(now += 40, 0);
  assert(game.paddleX() == stopped);
  game.confirm(now);
  int fromY = game.ballY();
  game.tick(now += 40, 0);
  assert(game.ballY() < fromY);
  game.confirm(now);
  int paused = game.ballY();
  game.tick(now += 1000, 0);
  assert(game.phase() == Phase::Paused && game.ballY() == paused);
  game.confirm(now);
  game.tick(now += 40, 0);
  assert(game.ballY() < paused);
  game.suspend();
  assert(game.phase() == Phase::Paused);

  game.open(now);
  for (int remaining = 2; remaining >= 0; --remaining) {
    game.confirm(now);
    for (int i = 0; i < 240 && game.lives() > remaining; ++i)
      game.tick(now += 8, -1);
    assert(game.lives() == remaining && game.score() > 0);
    assert(game.phase() == (remaining ? Phase::Ready : Phase::Lost));
  }
  game.confirm(now);
  assert(game.lives() == 3 && game.score() == 0 && game.phase() == Phase::Ready);
}

int main() {
  for (int difficulty = 0; difficulty < Minesweeper::LevelCount; ++difficulty)
    for (uint32_t seed = 0; seed < 150; ++seed)
      verifyMines(difficulty, seed);
  verifyMineLoss();
  verifyBreakout();
  std::cout << "PASS: Minesweeper layouts, flood fill, flags, first turn, win/loss and Breakout controls\n";
}
