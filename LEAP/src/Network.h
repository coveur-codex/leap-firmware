#pragma once
#include "Assets.h"
#include "Ota.h"
#include <WiFi.h>
#include <atomic>
namespace leap {
struct KnowledgeRequest {
  char path[512];
};
class Network {
  QueueHandle_t requests = nullptr;
  TaskHandle_t task = nullptr;
  Transport net;
  String base;
  bool sync();
  bool event(const String &id, const char *name, JsonDocument &state, JsonDocument &response,
             const String &package = "", int version = 0, const String &message = "");
  void run();
  void checkin(JsonDocument &state);
  void content(JsonDocument &state);

public:
  std::atomic<bool> connected{false}, busy{false}, requested{false};
  std::atomic<bool> timeSynced{false}, selfTestDone{false}, selfTestPassed{false};
  std::atomic<uint32_t> knowledgeRevision{0};
  bool begin();
  bool knowledge(const String &suffix);
  void requestSync() {
    requested = true;
  }
};
extern Network network;
} // namespace leap
