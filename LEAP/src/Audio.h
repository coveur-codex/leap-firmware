#pragma once
#include "Storage.h"
#include <atomic>
namespace leap {
struct AudioJob {
  char path[100];
  uint16_t frequency, duration;
};
class Audio {
  QueueHandle_t jobs = nullptr;

public:
  std::atomic<uint8_t> volume{35};
  std::atomic<bool> stopRequested{false};
  bool begin();
  void tone(uint16_t hz, uint16_t duration = 120);
  bool play(const String &blob);
  void stop() {
    stopRequested = true;
  }
  static bool validateWav(const String &blob);

private:
  void run();
};
extern Audio audio;
} // namespace leap
