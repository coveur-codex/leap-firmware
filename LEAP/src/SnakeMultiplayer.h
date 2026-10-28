#pragma once
#include "Storage.h"
#include "SnakeGrid.h"
#include <atomic>
#include <freertos/queue.h>
namespace leap {
enum class SnakePhase : uint8_t { Connecting, Lobby, Countdown, Playing, Alone, Finished, Error };
struct SnakeCell { uint8_t x = 0, y = 0; };
struct SnakePlayer {
  char id[81]{}, avatar[101]{}, accent[8]{};
  uint16_t offset = 0, length = 0;
  uint32_t seq = 0, respawnMs = 0;
  uint8_t direction = 3;
  bool connected = false, protectedSpawn = false;
};
struct SnakeSnapshot {
  uint32_t epoch = 0, revision = 0, tick = 0, remainingMs = 0;
  char session[33]{}, error[161]{};
  SnakePhase phase = SnakePhase::Connecting;
  uint16_t score = 0, target = 0;
  uint8_t count = 0, foodCount = 0;
  uint8_t columns = SnakeGrid::Columns, rows = SnakeGrid::Rows;
  SnakePlayer players[4];
  SnakeCell cells[SnakeGrid::Capacity], food[10];
};
// Pure bounded decoder shared by the socket and native protocol tests.
bool decodeSnake(JsonVariantConst frame, SnakeSnapshot &out);
class SnakeMultiplayer {
  QueueHandle_t incoming = nullptr, outgoing = nullptr;
  std::atomic<bool> enabled{false};
  std::atomic<uint32_t> epoch{0};
  bool ready = false;
  void run();
public:
  std::atomic<bool> online{false};
  bool begin();
  bool start();
  void close();
  void turn(uint8_t direction, uint32_t tick);
  bool poll(SnakeSnapshot &out);
  void service(uint32_t now, bool connected);
};
extern SnakeMultiplayer snakeMultiplayer;
} // namespace leap
