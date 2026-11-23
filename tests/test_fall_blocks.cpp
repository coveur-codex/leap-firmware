#include "FallBlocks.h"
#include <cassert>
#include <cstdio>

using leap::FallBlocks;

static int occupied(const FallBlocks &game) {
  int count = 0;
  for (int y = 0; y < FallBlocks::Rows; ++y)
    for (int x = 0; x < FallBlocks::Columns; ++x)
      count += game.cell(x, y) != FallBlocks::Empty;
  return count;
}

int main() {
  FallBlocks game;
  assert(game.phase() == FallBlocks::Phase::Ready);
  game.hardDrop(0);
  assert(occupied(game) == 0);

  uint32_t time = UINT32_MAX - 50;
  game.start(3, time);
  game.tick(time + game.fallInterval() - 1, false);
  assert(game.piece().y == 0);
  time += game.fallInterval();
  game.tick(time, false);
  assert(game.piece().y == 1 && game.score() == 0);
  int landing = game.landingRow();
  while (game.piece().y < landing) {
    time += FallBlocks::SoftDropMs;
    game.tick(time, true);
  }
  assert(game.score() == landing - 1 && occupied(game) == 0);
  auto next = game.next();
  game.tick(time + FallBlocks::LockMs - 1, false);
  assert(occupied(game) == 0);
  game.tick(time + FallBlocks::LockMs, false);
  assert(occupied(game) == 4 && game.piece().shape == next && game.piece().y == 0);

  game.start(3, 0);
  assert(game.piece().shape == FallBlocks::Bar && game.rotate(0));
  while (game.shift(-1, 0)) {}
  assert(game.piece().x == -2);
  assert(game.rotate(0) && game.piece().x == 0 && !game.shift(-1, 0));

  // The first six shapes for this seed stack into two complete bottom rows.
  game.start(3, 0);
  struct Placement { int turns, column; };
  const Placement placements[] = {{0, 0}, {0, 3}, {0, 0}, {0, 7}, {3, 5}, {1, 6}};
  int dropPoints = 0;
  for (auto [turns, x] : placements) {
    for (int turn = 0; turn < turns; ++turn)
      assert(game.rotate(0));
    while (game.piece().x < x)
      assert(game.shift(1, 0));
    while (game.piece().x > x)
      assert(game.shift(-1, 0));
    dropPoints += 2 * (game.landingRow() - game.piece().y);
    game.hardDrop(0);
  }
  assert(occupied(game) == 4 && game.score() == dropPoints + 250);

  for (uint32_t seed = 1; seed <= 20; ++seed) {
    game.start(seed, 0);
    for (int bag = 0; bag < 2; ++bag) {
      unsigned used = 0;
      for (int i = 0; i < 7; ++i) {
        assert(game.phase() == FallBlocks::Phase::Playing);
        auto bit = 1u << game.piece().shape;
        assert(!(used & bit));
        used |= bit;
        int target = (bag * 7 + i) % 2 == 0 ? 0 : 6;
        while (game.piece().x < target)
          assert(game.shift(1, 0));
        while (game.piece().x > target)
          assert(game.shift(-1, 0));
        game.hardDrop(0);
      }
      assert(used == 0xfe);
    }
  }

  game.start(3, 0);
  for (int i = 0; i < 20 && game.phase() == FallBlocks::Phase::Playing; ++i)
    game.hardDrop(0);
  assert(game.phase() == FallBlocks::Phase::Over);
  int score = game.score();
  game.hardDrop(0);
  assert(game.score() == score);
  game.start(0, 0);
  assert(game.phase() == FallBlocks::Phase::Playing && game.score() == 0 && occupied(game) == 0);
  std::puts("PASS: Fallblöcke gravity, lock delay, wall rotation, double row, bag and restart");
}
