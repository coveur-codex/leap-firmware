#pragma once
#include "Storage.h"

namespace leap {
// Unsigned subtraction also handles one millis() rollover.
class QuizTimer {
  uint32_t started = 0;
  bool visible = false;
public:
  void reset() { visible = false; }
  void shown(uint32_t now) {
    if (!visible) { started = now; visible = true; }
  }
  bool active() const { return visible; }
  bool accepts(uint32_t click) const { return visible && int32_t(click - started) >= 0; }
  uint32_t duration(uint32_t now) const { return now - started; }
};

// Capture the displayed permutation, never reconstruct it later on the server.
inline void quizAnswerSnapshot(JsonDocument &out, JsonVariantConst question,
                               const int (&order)[4], int selected, uint32_t elapsed) {
  out["question"] = question["q"];
  out["questionId"] = question["id"];
  auto answers = out["answers"].to<JsonArray>();
  for (int i = 0; i < 4; ++i) {
    answers.add(question["a"][order[i]]);
    if (order[i] == 0) out["correctIndex"] = i;
  }
  out["selectedIndex"] = selected;
  out["elapsedMs"] = elapsed;
}

class QuizTracking {
  SemaphoreHandle_t mutex = nullptr;
  JsonDocument pending{&jsonRam};
  bool ready = false;
public:
  static constexpr size_t MaxAttempts = 128, MaxBytes = 512 * 1024;
  bool begin();
  bool enqueue(JsonDocument &attempt);
  bool front(JsonDocument &attempt);
  bool acknowledge(const String &eventId);
};
extern QuizTracking quizTracking;
} // namespace leap
