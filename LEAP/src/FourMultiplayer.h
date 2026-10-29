#pragma once
#include "Storage.h"
#include <atomic>
#include <freertos/queue.h>
namespace leap {
enum class FourPhase : uint8_t { Connecting, Lobby, Playing, Paused, Finished, Error };
struct FourPlayer {
  char id[81]{}, name[101]{}, accent[8]{};
  uint32_t seq = 0;
  bool connected = false, rematch = false, left = false;
};
struct FourSnapshot {
  uint32_t epoch = 0, revision = 0, remainingMs = 0, round = 0, move = 0;
  char session[33]{}, reason[32]{}, error[100]{};
  FourPhase phase = FourPhase::Connecting;
  uint8_t count = 0, turn = 0, winner = 0;
  uint8_t board[6][7]{};
  FourPlayer players[2];
};
bool decodeFour(JsonVariantConst frame, FourSnapshot &out);
class FourMultiplayer {
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
  void command(uint8_t column, uint32_t round, uint32_t move, bool rematch = false);
  bool poll(FourSnapshot &out);
  void service(uint32_t now, bool connected);
};
extern FourMultiplayer fourMultiplayer;
} // namespace leap
