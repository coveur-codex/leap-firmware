#pragma once
#include "Storage.h"
#include "rangierwerk/Yard.h"
#include <atomic>
#include <freertos/queue.h>

namespace leap {
enum class RangierPhase : uint8_t { Connecting, Playing, Full, Error };
enum class RangierAction : uint8_t { Turnout, Split, Recover };

struct RangierFrame {
  uint32_t epoch = 0, card = 0, seed = 0, remaining = 0, switches = 0, delivered = 0;
  uint16_t colors[rangierwerk::Engines]{};
  char names[rangierwerk::Engines][21]{};
  uint8_t connected = 0;
  rangierwerk::Index difficulty = 1, self = rangierwerk::None;
  RangierPhase phase = RangierPhase::Connecting;
  bool summary = false;
  std::array<rangierwerk::Vehicle, rangierwerk::MaxVehicles> vehicles{};
  std::array<rangierwerk::Order, rangierwerk::MaxOrders> orders{};
};

bool decodeRangierFrame(JsonVariantConst json, RangierFrame &out);

class RangierwerkMultiplayer {
  QueueHandle_t incoming = nullptr, outgoing = nullptr;
  std::atomic<bool> enabled{false};
  std::atomic<uint32_t> epoch{0}, connection{0};
  std::atomic<int> direction{0};
  bool ready = false;
  bool begin();
  void service();
  void run();

public:
  std::atomic<bool> online{false};
  bool start();
  void close();
  void throttle(int value) { direction = value; }
  void action(RangierAction type, uint32_t card, uint32_t a = 0, uint32_t b = 0);
  bool poll(RangierFrame &out);
};
extern RangierwerkMultiplayer rangierwerkMultiplayer;
} // namespace leap
