#pragma once
#include "PondRace.h"
#include <ArduinoJson.h>
#include <atomic>
#include <Arduino.h>
#include <freertos/queue.h>

namespace leap {
bool decodePondFrame(JsonVariantConst doc, PondFrame &frame);

class PondMultiplayer {
  QueueHandle_t incoming = nullptr, outgoing = nullptr;
  std::atomic<bool> enabled{false};
  std::atomic<uint32_t> epoch{0}, connection{0};
  bool initialized = false;
  bool begin();
  void run();
  void service(uint32_t now, bool connected);

public:
  std::atomic<bool> online{false};
  bool start();
  void close();
  bool action(PondAction action, uint32_t turnId);
  bool poll(PondFrame &frame);
};
extern PondMultiplayer pondMultiplayer;
} // namespace leap
