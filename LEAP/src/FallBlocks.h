#pragma once
#include "Core.h"
#include <array>

namespace leap {
class FallBlocks {
public:
  static constexpr int Columns = 10, Rows = 20;
  static constexpr int MaxScore = 999999999;
  static constexpr uint32_t SoftDropMs = 65, LockMs = 360;
  enum Shape : uint8_t { Empty, Bar, Square, Tee, Step, ReverseStep, Hook, ReverseHook };
  enum class Phase { Ready, Playing, Over };
  struct Cell { int x, y; };
  struct Piece {
    Shape shape = Empty;
    int x = 3, y = 0, turns = 0;
  };

  static Cell block(Shape shape, int turns, int index) {
    constexpr Cell starts[][4] = {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
    };
    Cell cell = starts[shape - 1][index];
    if (shape != Square)
      for (int i = 0; i < turns; ++i)
        cell = {(shape == Bar ? 3 : 2) - cell.y, cell.x};
    return cell;
  }

  void start(uint32_t seed, uint32_t now) {
    *this = FallBlocks{};
    randomState = seed ? seed : 1;
    status = Phase::Playing;
    upcoming = takeShape();
    spawn(now);
  }

  Shape cell(int x, int y) const { return board[y][x]; }
  const Piece &piece() const { return active; }
  Shape next() const { return upcoming; }
  Phase phase() const { return status; }
  int score() const { return points; }
  int level() const { return 1 + std::min(rowsCleared / 8, 11); }
  uint32_t fallInterval() const { return 760 - 58 * (level() - 1); }

  bool shift(int direction, uint32_t now) {
    if (status != Phase::Playing)
      return false;
    Piece moved = active;
    moved.x += direction;
    return place(moved, now);
  }

  bool rotate(uint32_t now) {
    if (status != Phase::Playing || active.shape == Square)
      return false;
    Piece turned = active;
    turned.turns = (active.turns + 1) % 4;
    constexpr Cell offsets[] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0},
                                {0, -1}, {-1, -1}, {1, -1}};
    for (Cell offset : offsets) {
      turned.x = active.x + offset.x;
      turned.y = active.y + offset.y;
      if (place(turned, now))
        return true;
    }
    return false;
  }

  int landingRow() const {
    if (status != Phase::Playing)
      return active.y;
    Piece landing = active;
    while (fits(landing, 1))
      ++landing.y;
    return landing.y;
  }

  void softDrop(uint32_t now) {
    if (status == Phase::Playing)
      descend(now, true);
  }

  void hardDrop(uint32_t now) {
    if (status != Phase::Playing)
      return;
    int row = landingRow();
    addPoints(2 * (row - active.y));
    active.y = row;
    lock(now);
  }

  void tick(uint32_t now, bool soft) {
    if (status != Phase::Playing)
      return;
    if (grounded && elapsed(now, groundedAt, LockMs))
      lock(now);
    else if (elapsed(now, fellAt, soft ? SoftDropMs : fallInterval()))
      descend(now, soft);
  }

private:
  using Row = std::array<Shape, Columns>;
  std::array<Row, Rows> board{};
  std::array<Shape, 7> bag{};
  Piece active;
  Shape upcoming = Empty;
  Phase status = Phase::Ready;
  int points = 0, rowsCleared = 0;
  size_t bagIndex = bag.size();
  uint32_t randomState = 1, fellAt = 0, groundedAt = 0;
  bool grounded = false;

  uint32_t random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
  }

  Shape takeShape() {
    if (bagIndex == bag.size()) {
      for (size_t i = 0; i < bag.size(); ++i)
        bag[i] = static_cast<Shape>(i + 1);
      for (size_t i = bag.size() - 1; i > 0; --i)
        std::swap(bag[i], bag[random() % (i + 1)]);
      bagIndex = 0;
    }
    return bag[bagIndex++];
  }

  bool fits(const Piece &piece, int dy = 0) const {
    for (int i = 0; i < 4; ++i) {
      Cell tile = block(piece.shape, piece.turns, i);
      int x = tile.x + piece.x, y = tile.y + piece.y + dy;
      if (x < 0 || x >= Columns || y < 0 || y >= Rows || board[y][x] != Empty)
        return false;
    }
    return true;
  }

  void updateGrounded(uint32_t now) {
    bool touching = !fits(active, 1);
    if (touching && !grounded)
      groundedAt = now;
    grounded = touching;
  }

  bool place(const Piece &piece, uint32_t now) {
    if (!fits(piece))
      return false;
    active = piece;
    updateGrounded(now);
    return true;
  }

  void descend(uint32_t now, bool soft) {
    fellAt = now;
    Piece moved = active;
    ++moved.y;
    if (place(moved, now) && soft)
      addPoints(1);
  }

  void addPoints(int value) { points = std::min(points + value, MaxScore); }

  void spawn(uint32_t now) {
    active = Piece{upcoming};
    upcoming = takeShape();
    fellAt = now;
    grounded = false;
    if (fits(active))
      updateGrounded(now);
    else
      status = Phase::Over;
  }

  void lock(uint32_t now) {
    for (int i = 0; i < 4; ++i) {
      Cell tile = block(active.shape, active.turns, i);
      board[active.y + tile.y][active.x + tile.x] = active.shape;
    }
    int destination = Rows - 1;
    for (int row = Rows - 1; row >= 0; --row) {
      if (std::find(board[row].begin(), board[row].end(), Empty) != board[row].end())
        board[destination--] = board[row];
    }
    int cleared = destination + 1;
    while (destination >= 0)
      board[destination--].fill(Empty);
    constexpr int RowPoints[] = {0, 100, 250, 450, 700};
    addPoints(RowPoints[cleared] * level());
    rowsCleared += cleared;
    spawn(now);
  }
};
} // namespace leap
