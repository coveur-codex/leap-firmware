#pragma once
#include <cstdint>
namespace leap {
class MazeState {
public:
  static constexpr int Size = 9;
  bool open[Size][Size]{};
  int x = 0, y = 0;
  bool won = false;
  void start(uint32_t seed) {
    for (auto &row : open)
      for (bool &cell : row) cell = false;
    x = y = 0;
    won = false;
    struct Cell { int x, y; };
    Cell stack[25]{};
    int depth = 1;
    open[0][0] = true;
    seed = seed ? seed : 0x9e3779b9u;
    while (depth) {
      Cell current = stack[depth - 1], choices[4];
      int count = 0;
      const int dx[] = {0, 0, -2, 2}, dy[] = {-2, 2, 0, 0};
      for (int i = 0; i < 4; ++i) {
        int nx = current.x + dx[i], ny = current.y + dy[i];
        if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && !open[ny][nx])
          choices[count++] = {nx, ny};
      }
      if (!count) { --depth; continue; }
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      Cell next = choices[seed % count];
      open[(current.y + next.y) / 2][(current.x + next.x) / 2] = true;
      open[next.y][next.x] = true;
      stack[depth++] = next;
    }
  }
  void move(int direction) {
    if (won || direction < 0 || direction > 3) return;
    int nx = x + (direction == 3) - (direction == 2);
    int ny = y + (direction == 1) - (direction == 0);
    if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && open[ny][nx]) {
      x = nx; y = ny;
    }
    won = x == Size - 1 && y == Size - 1;
  }
};
} // namespace leap
