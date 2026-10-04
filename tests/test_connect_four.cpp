#include "ConnectFour.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace leap;
using P = ConnectFour;
int main() {
  P board;
  assert(!board.drop(-1, P::Human) && !board.drop(7, P::Device));
  assert(!board.drop(0, P::Empty));
  for (int i = 0; i < 6; ++i) {
    assert(board.drop(2, i % 2 ? P::Device : P::Human));
    assert(board.cells[5 - i][2] == (i % 2 ? P::Device : P::Human));
  }
  assert(!board.drop(2, P::Human) && board.moves == 6);
  for (auto level : {P::Difficulty::Easy, P::Difficulty::Medium, P::Difficulty::Hard}) {
    P before = board;
    int move = board.choose(level, 2);
    assert(board.legal(move) && move != 2);
    assert(std::memcmp(before.cells, board.cells, sizeof(board.cells)) == 0);
    assert(board.moves == before.moves && board.winner == before.winner);
  }
  for (int direction = 0; direction < 4; ++direction) {
    P win;
    for (int i = 0; i < 4; ++i) {
      int col = direction == 1 ? 0 : i;
      int support = direction == 2 ? i : direction == 3 ? 3 - i : 0;
      for (int j = 0; j < support; ++j)
        assert(win.drop(col, P::Device));
      assert(win.drop(col, P::Human));
    }
    assert(win.finished && win.winner == P::Human);
    assert(!win.drop(6, P::Device));
    assert(win.choose(P::Difficulty::Hard, 0) == -1);
  }
  // Winning takes precedence over blocking; every supported tactical level does both.
  for (auto level : {P::Difficulty::Medium, P::Difficulty::Hard}) {
    P threat;
    for (int i = 0; i < 3; ++i)
      threat.drop(6, P::Human);
    assert(threat.choose(level, 0) == 6);
    for (int i = 0; i < 3; ++i)
      threat.drop(0, P::Device);
    assert(threat.choose(level, 0) == 0);
    threat.drop(0, P::Device);
    assert(threat.finished && threat.winner == P::Device);
  }
  // Full board with no four in any direction.
  P draw;
  for (int c = 0; c < 7; ++c)
    for (int r = 5; r >= 0; --r)
      assert(draw.drop(c, ((c / 2 + r) % 2) ? P::Human : P::Device));
  assert(draw.moves == 42 && draw.finished && draw.winner == P::Empty);
  assert(draw.choose(P::Difficulty::Easy, 0) == -1);
  // Exercise complete legal matches on every level, including late-game positions.
  for (auto level : {P::Difficulty::Easy, P::Difficulty::Medium, P::Difficulty::Hard})
    for (uint32_t seed = 1; seed <= 12; ++seed) {
      P match;
      uint32_t random = seed;
      while (!match.finished) {
        random = random * 1664525u + 1013904223u;
        int col = match.choose(P::Difficulty::Easy, random);
        assert(match.drop(col, P::Human));
        if (match.finished)
          break;
        assert(match.drop(match.choose(level, random), P::Device));
      }
      assert(match.moves <= 42);
    }
  std::cout << "PASS: Connect Four gravity, full columns, all wins, draw and AI legality/tactics\n";
}
