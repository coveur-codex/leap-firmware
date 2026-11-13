#pragma once
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>
class String : public std::string {
public:
  using std::string::string;
  String(const std::string &s) : std::string(s) {}
  bool startsWith(const char *s) const { return rfind(s, 0) == 0; }
  int indexOf(char value) const { auto pos = find(value); return pos == npos ? -1 : int(pos); }
  String substring(size_t start, size_t end) const { return substr(start, end - start); }
  void toLowerCase() { for (char &c : *this) if (c >= 'A' && c <= 'Z') c += 32; }
  void trim() { auto first = find_first_not_of(" \r\n\t"), last = find_last_not_of(" \r\n\t"); *this = first == npos ? "" : substr(first, last - first + 1); }
};
inline size_t strlcpy(char *out, const char *in, size_t cap) {
  auto n = strlen(in); if (cap) { auto count = std::min(n, cap - 1); memcpy(out, in, count); out[count] = 0; } return n;
}
inline std::string radioSerial;
struct FakeSerial {
  template<class... Args> void printf(const char *format, Args... args) {
    char buffer[1024]; snprintf(buffer, sizeof(buffer), format, args...); radioSerial += buffer;
  }
};
inline FakeSerial Serial;
struct FakeEsp { unsigned getFreeHeap() { return 80000; } unsigned getFreePsram() { return 7000000; } };
inline FakeEsp ESP;
using TickType_t = uint32_t;
constexpr int pdTRUE = 1, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
inline uint32_t radioNow = 0;
inline uint32_t millis() { return radioNow; }
inline TickType_t pdMS_TO_TICKS(uint32_t value) { return value; }
inline void vTaskDelay(uint32_t value) { radioNow += value; }
inline unsigned uxTaskGetStackHighWaterMark(void *) { return 4000; }
struct FakeQueue { size_t size; std::deque<std::vector<uint8_t>> commands; };
using QueueHandle_t = FakeQueue *;
using SemaphoreHandle_t = void *;
inline std::vector<std::unique_ptr<FakeQueue>> radioQueues;
inline void (*radioTask)(void *) = nullptr;
inline void *radioContext = nullptr;
struct WorkerIdle {};
inline QueueHandle_t xQueueCreate(unsigned, unsigned size) {
  radioQueues.emplace_back(new FakeQueue{size,{}}); return radioQueues.back().get();
}
inline int xQueueOverwrite(QueueHandle_t queue, const void *value) {
  auto bytes = static_cast<const uint8_t *>(value); queue->commands.clear();
  queue->commands.emplace_back(bytes, bytes + queue->size); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void *out, TickType_t) {
  if (queue->commands.empty()) throw WorkerIdle{};
  memcpy(out, queue->commands.front().data(), queue->size); queue->commands.pop_front(); return pdTRUE;
}
inline void vQueueDelete(QueueHandle_t) {}
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
inline int xTaskCreatePinnedToCore(void (*task)(void *), const char *, unsigned, void *context, unsigned, void *, int) {
  radioTask = task; radioContext = context; return pdPASS;
}
