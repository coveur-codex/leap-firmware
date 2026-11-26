#pragma once
#include <algorithm>
#include <cstdint>

namespace leap {
class Minesweeper {
public:
  struct Level { const char *name; int columns, rows, mines; };
  inline static constexpr Level Levels[] = {
      {"Leicht", 12, 5, 7}, {"Mittel", 16, 6, 12}, {"Schwer", 20, 7, 24}};
  static constexpr int LevelCount = sizeof(Levels) / sizeof(Levels[0]);
  enum class Phase { Choose, Playing, Won, Lost };
  enum class Cover : uint8_t { Covered, Flagged, Open };
  struct Cell {
    Cover cover = Cover::Covered;
    uint8_t neighbors = 0;
    bool mine = false;
  };

  void open() { phase_ = Phase::Choose; }
  Phase phase() const { return phase_; }
  int difficulty() const { return difficulty_; }
  const Level &level() const { return Levels[difficulty_]; }
  const Cell &cell(int x, int y) const { return cells_[y * level().columns + x]; }
  int cursorX() const { return x_; }
  int cursorY() const { return y_; }
  int flagged() const { return flagged_; }

  void move(int dx, int dy) {
    if (phase_ == Phase::Choose)
      difficulty_ = std::clamp(difficulty_ + dx + dy, 0, LevelCount - 1);
    if (phase_ == Phase::Playing) {
      x_ = std::clamp(x_ + dx, 0, level().columns - 1);
      y_ = std::clamp(y_ + dy, 0, level().rows - 1);
    }
  }

  bool flag() {
    if (phase_ != Phase::Playing)
      return false;
    auto &tile = cells_[y_ * level().columns + x_].cover;
    if (tile == Cover::Open)
      return false;
    bool covered = tile == Cover::Covered;
    tile = covered ? Cover::Flagged : Cover::Covered;
    flagged_ += covered ? 1 : -1;
    return true;
  }

  bool confirm(uint32_t seed) {
    if (phase_ != Phase::Playing) {
      x_ = level().columns / 2;
      y_ = level().rows / 2;
      remaining_ = level().columns * level().rows - level().mines;
      flagged_ = 0;
      placed_ = false;
      for (auto &cell : cells_)
        cell = {};
      phase_ = Phase::Playing;
      return true;
    }
    int index = y_ * level().columns + x_;
    auto &target = cells_[index];
    if (target.cover != Cover::Covered)
      return false;
    if (!placed_)
      place(seed);
    target.cover = Cover::Open;
    if (target.mine) {
      phase_ = Phase::Lost;
      return true;
    }

    // Open each cell when it enters the queue so it is processed at most once.
    uint8_t queue[MaxCells];
    int head = 0, tail = 0;
    queue[tail++] = uint8_t(index);
    while (head < tail) {
      int current = queue[head++];
      if (cells_[current].neighbors)
        continue;
      adjacent(current, [&](int neighbor) {
        if (cells_[neighbor].cover == Cover::Covered) {
          cells_[neighbor].cover = Cover::Open;
          queue[tail++] = uint8_t(neighbor);
        }
      });
    }
    remaining_ -= tail;
    if (!remaining_)
      phase_ = Phase::Won;
    return true;
  }

private:
  static constexpr int MaxCells = 20 * 7;
  Phase phase_ = Phase::Choose;
  Cell cells_[MaxCells]{};
  int difficulty_ = 1, x_ = 0, y_ = 0, remaining_ = 0, flagged_ = 0;
  bool placed_ = false;

  template <typename Visit> void adjacent(int index, Visit visit) const {
    int x = index % level().columns, y = index / level().columns;
    for (int ny = std::max(0, y - 1); ny <= std::min(y + 1, level().rows - 1); ++ny)
      for (int nx = std::max(0, x - 1); nx <= std::min(x + 1, level().columns - 1); ++nx)
        if (nx != x || ny != y)
          visit(ny * level().columns + nx);
  }

  void place(uint32_t seed) {
    uint8_t candidates[MaxCells];
    int count = 0;
    for (int y = 0; y < level().rows; ++y)
      for (int x = 0; x < level().columns; ++x)
        if (x < x_ - 1 || x > x_ + 1 || y < y_ - 1 || y > y_ + 1)
          candidates[count++] = uint8_t(y * level().columns + x);
    for (int i = 0; i < level().mines; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int selected = seed % count;
      int index = candidates[selected];
      candidates[selected] = candidates[--count];
      cells_[index].mine = true;
      adjacent(index, [&](int neighbor) { ++cells_[neighbor].neighbors; });
    }
    placed_ = true;
  }
};
} // namespace leap
