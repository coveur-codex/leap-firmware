#pragma once
// Deterministic GPIO/FreeRTOS harness for Input.h; no device timing claims.
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>
constexpr int INPUT_PULLUP = 1, LOW = 0;
constexpr int pdTRUE = 1, pdPASS = 1;
using TickType_t = uint32_t;
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
struct FakeQueue {
  unsigned capacity, itemSize;
  std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t = FakeQueue *;
inline std::vector<std::unique_ptr<FakeQueue>> fakeQueues;
inline uint32_t fakeNow = 0, fakeUntil = 100;
inline bool fakeTaskFailure = false;
inline void (*fakeTask)(void *) = nullptr;
inline void *fakeContext = nullptr;
inline bool (*fakeDown)(int, uint32_t) = nullptr;
inline void pinMode(int, int) {}
inline uint32_t millis() { return fakeNow; }
inline int digitalRead(int pin) { return fakeDown(pin, fakeNow) ? LOW : 1; }
inline QueueHandle_t xQueueCreate(unsigned count, unsigned size) {
  fakeQueues.emplace_back(new FakeQueue{count, size, {}});
  return fakeQueues.back().get();
}
inline int xQueueSend(QueueHandle_t q, const void *item, TickType_t) {
  if (q->items.size() == q->capacity) return 0;
  auto p = static_cast<const uint8_t *>(item);
  q->items.emplace_back(p, p + q->itemSize);
  return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void *item, TickType_t) {
  if (q->items.empty()) return 0;
  memcpy(item, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  return pdTRUE;
}
inline void vQueueDelete(QueueHandle_t q) {
  for (auto &owned : fakeQueues)
    if (owned.get() == q) { owned.reset(); break; }
}
inline int xTaskCreatePinnedToCore(void (*fn)(void *), const char *, unsigned, void *context,
                                 unsigned, void *, int) {
  if (fakeTaskFailure) return 0;
  fakeTask = fn;
  fakeContext = context;
  return pdPASS;
}
inline TickType_t xTaskGetTickCount() { return fakeNow; }
struct SamplingFinished {};
inline void vTaskDelayUntil(TickType_t *wake, TickType_t increment) {
  *wake += increment;
  fakeNow = *wake;
  if (fakeNow > fakeUntil) throw SamplingFinished{};
}
