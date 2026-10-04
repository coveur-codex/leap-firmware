#pragma once
#include <algorithm>
#include <cstdint>
namespace leap {
class ConnectFour {
public:
  static constexpr int Columns = 7, Rows = 6;
  enum Player : uint8_t { Empty, Human, Device };
  enum class Difficulty { Easy, Medium, Hard };
  uint8_t cells[Rows][Columns]{};
  uint8_t heights[Columns]{};
  int moves = 0;
  Player winner = Empty;
  bool finished = false;

  bool legal(int column) const {
    return column >= 0 && column < Columns && heights[column] < Rows && !finished;
  }
  bool drop(int column, Player player) {
    if (!legal(column) || (player != Human && player != Device))
      return false;
    int row = Rows - 1 - heights[column]++;
    cells[row][column] = player;
    ++moves;
    const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (auto &d : directions) {
      int count = 1;
      for (int sign : {-1, 1}) {
        int x = column + sign * d[0], y = row + sign * d[1];
        while (x >= 0 && x < Columns && y >= 0 && y < Rows && cells[y][x] == player) {
          ++count;
          x += sign * d[0];
          y += sign * d[1];
        }
      }
      if (count >= 4)
        winner = player;
    }
    finished = winner != Empty || moves == Rows * Columns;
    return true;
  }
  // A node budget bounds work; the six-ply limit bounds stack use on the ESP32.
  int choose(Difficulty difficulty, uint32_t random) const {
    int legalColumns[Columns], count = 0;
    for (int c = 0; c < Columns; ++c)
      if (legal(c))
        legalColumns[count++] = c;
    if (!count)
      return -1;
    if (difficulty == Difficulty::Easy)
      return legalColumns[random % count];
    for (Player player : {Device, Human})
      for (int c : order) {
        ConnectFour next = *this;
        if (next.drop(c, player) && next.winner == player)
          return c;
      }
    int best = -1000000, column = legalColumns[0];
    for (int c : order) {
      ConnectFour next = *this;
      if (!next.drop(c, Device))
        continue;
      int budget = 2000; // Equal work per candidate, at most 14,000 search nodes.
      int value = next.search(difficulty == Difficulty::Hard ? 5 : 1, Human,
                              -1000000, 1000000, budget);
      if (value > best) {
        best = value;
        column = c;
      }
    }
    return column;
  }

private:
  inline static constexpr int order[Columns] = {3, 2, 4, 1, 5, 0, 6};
  int evaluate() const {
    int score = 0;
    for (int r = 0; r < Rows; ++r)
      score += cells[r][3] == Device ? 6 : cells[r][3] == Human ? -6 : 0;
    const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int r = 0; r < Rows; ++r)
      for (int c = 0; c < Columns; ++c)
        for (auto &d : directions) {
          int endX = c + 3 * d[0], endY = r + 3 * d[1];
          if (endX < 0 || endX >= Columns || endY < 0 || endY >= Rows)
            continue;
          int device = 0, human = 0;
          for (int i = 0; i < 4; ++i) {
            device += cells[r + i * d[1]][c + i * d[0]] == Device;
            human += cells[r + i * d[1]][c + i * d[0]] == Human;
          }
          const int weights[] = {0, 1, 10, 60, 0};
          if (!human)
            score += weights[device];
          if (!device)
            score -= weights[human];
        }
    return score;
  }
  int search(int depth, Player turn, int alpha, int beta, int &budget) const {
    --budget;
    if (winner != Empty)
      return winner == Device ? 100000 + depth : -100000 - depth;
    if (finished)
      return 0;
    if (depth == 0 || budget <= 0)
      return evaluate();
    int best = turn == Device ? -1000000 : 1000000;
    for (int c : order) {
      ConnectFour next = *this;
      if (!next.drop(c, turn))
        continue;
      int value = next.search(depth - 1, turn == Device ? Human : Device, alpha, beta, budget);
      if (turn == Device) {
        best = std::max(best, value);
        alpha = std::max(alpha, best);
      } else {
        best = std::min(best, value);
        beta = std::min(beta, best);
      }
      if (alpha >= beta || budget <= 0)
        break;
    }
    return best;
  }
};
} // namespace leap
