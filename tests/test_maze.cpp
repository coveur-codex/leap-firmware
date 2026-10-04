#include "Maze.h"
#include <cassert>
#include <cstring>
#include <queue>
#include <set>
#include <string>
#include <iostream>
using namespace leap;
int main() {
  std::set<std::string> layouts;
  for (uint32_t seed = 0; seed < 100; ++seed) {
    MazeState maze;
    maze.start(seed);
    MazeState repeat;
    repeat.start(seed);
    assert(std::memcmp(maze.open, repeat.open, sizeof(maze.open)) == 0);
    std::string layout;
    int passages = 0;
    for (auto &row : maze.open)
      for (bool cell : row) { layout += cell ? '1' : '0'; passages += cell; }
    layouts.insert(layout);
    assert(passages == 49 && maze.open[0][0] && maze.open[8][8]);
    int parent[81], direction[81];
    for (int &v : parent) v = -1;
    std::queue<int> pending;
    pending.push(0); parent[0] = 0;
    const int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
    int reached = 0;
    while (!pending.empty()) {
      int cell = pending.front(); pending.pop(); ++reached;
      for (int d = 0; d < 4; ++d) {
        int x = cell % 9 + dx[d], y = cell / 9 + dy[d];
        if (x < 0 || x >= 9 || y < 0 || y >= 9 || !maze.open[y][x]) continue;
        int next = y * 9 + x;
        if (parent[next] >= 0) continue;
        parent[next] = cell; direction[next] = d; pending.push(next);
      }
    }
    assert(reached == passages && parent[80] >= 0);
    maze.move(0); maze.move(2); maze.move(4);
    assert(maze.x == 0 && maze.y == 0);
    std::vector<int> route;
    for (int cell = 80; cell; cell = parent[cell]) route.push_back(direction[cell]);
    for (auto i = route.rbegin(); i != route.rend(); ++i) maze.move(*i);
    assert(maze.won && maze.x == 8 && maze.y == 8);
    maze.start(seed);
    assert(!maze.won && maze.x == 0 && maze.y == 0);
  }
  assert(layouts.size() > 90);
  std::cout << "PASS: 100 random 9x9 mazes, all passages connected, goal reachable and restart\n";
}
