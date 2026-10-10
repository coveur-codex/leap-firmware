#pragma once
#include "Core.h"
#include "Storage.h"
#include <atomic>
#include <esp_now.h>
namespace leap {
struct ChatLine {
  String name, text, symbol;
  uint32_t at = 0;
};
class Radio {
  QueueHandle_t incoming = nullptr;
  SeenMessages seen;
  JsonDocument templates{&jsonRam};
  uint32_t version = 0, boot = 0, sequence = 0, lastSend = 0;
  uint8_t mac[6]{};
  String name;
  bool ready = false;
  static void receive(const esp_now_recv_info_t *, const uint8_t *, int);

public:
  std::atomic<bool> enabled{false};
  ChatLine history[8];
  size_t count = 0;
  uint32_t revision = 0;
  bool begin();
  void configure(JsonDocument &state);
  void poll();
  bool send(size_t index);
  JsonArrayConst messages() const {
    return templates["messages"].as<JsonArrayConst>();
  }
};
extern Radio radio;
} // namespace leap
