#include "QuizTracking.h"

namespace leap {
QuizTracking quizTracking;
static constexpr char QueuePath[] = "/quiz-attempts.json";

bool QuizTracking::begin() {
  mutex = xSemaphoreCreateMutex();
  if (!mutex || !storage.ready) return false;
  if (LittleFS.exists(QueuePath)) {
    if (!storage.readJson(QueuePath, pending, MaxBytes) ||
        !pending["attempts"].is<JsonArray>() || pending["attempts"].size() > MaxAttempts)
      return false; // Preserve damaged data for diagnosis, never overwrite it.
  } else
    pending["attempts"].to<JsonArray>();
  ready = true;
  return true;
}

bool QuizTracking::enqueue(JsonDocument &attempt) {
  if (!ready || attempt.overflowed() || !attempt["eventId"].is<const char *>()) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  JsonDocument next(&jsonRam);
  next.set(pending);
  bool ok = next["attempts"].size() < MaxAttempts;
  if (ok) {
    next["attempts"].as<JsonArray>().add(attempt);
    ok = storage.writeJson(QueuePath, next, MaxBytes);
    if (ok) pending = std::move(next);
  }
  xSemaphoreGive(mutex);
  return ok;
}

bool QuizTracking::front(JsonDocument &attempt) {
  if (!ready || xSemaphoreTake(mutex, 0) != pdTRUE) return false;
  bool ok = pending["attempts"].size() > 0;
  if (ok) attempt.set(pending["attempts"][0]);
  xSemaphoreGive(mutex);
  return ok;
}

bool QuizTracking::acknowledge(const String &eventId) {
  if (!ready) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = pending["attempts"].size() && pending["attempts"][0]["eventId"] == eventId;
  if (ok) {
    // Include answers enqueued during the HTTP request; only remove its head.
    JsonDocument next(&jsonRam);
    next.set(pending);
    next["attempts"].as<JsonArray>().remove(0);
    ok = storage.writeJson(QueuePath, next, MaxBytes);
    if (ok) pending = std::move(next);
  }
  xSemaphoreGive(mutex);
  return ok;
}
} // namespace leap
