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
  template<class Number, typename std::enable_if<std::is_integral<Number>::value, int>::type = 0>
  String(Number n) : std::string(std::to_string(n)) {}
  bool endsWith(const char *s) const { auto n = strlen(s); return size() >= n && compare(size()-n,n,s) == 0; }
  int indexOf(const char *value) const { auto pos = find(value); return pos == npos ? -1 : int(pos); }
  void remove(size_t pos) { erase(pos); }
  size_t write(uint8_t value) { push_back(char(value)); return 1; }
  size_t write(const uint8_t *data, size_t length) { append(reinterpret_cast<const char *>(data), length); return length; }
  bool startsWith(const char *s) const { return rfind(s, 0) == 0; }
  int indexOf(char value) const { auto pos = find(value); return pos == npos ? -1 : int(pos); }
  String substring(size_t start, size_t end) const { return substr(start, end - start); }
  void toLowerCase() { for (char &c : *this) if (c >= 'A' && c <= 'Z') c += 32; }
  void trim() { auto first = find_first_not_of(" \r\n\t"), last = find_last_not_of(" \r\n\t"); *this = first == npos ? "" : substr(first, last - first + 1); }
};
#include "../compat/Strlcpy.h"
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
inline std::function<void()> radioOnDelay;
inline void vTaskDelay(uint32_t value) { radioNow += value; if (radioOnDelay) radioOnDelay(); }
inline unsigned uxTaskGetStackHighWaterMark(void *) { return 4000; }
struct FakeQueue { size_t size; std::deque<std::vector<uint8_t>> commands; };
using QueueHandle_t = FakeQueue *;
using SemaphoreHandle_t = void *;
inline std::vector<std::unique_ptr<FakeQueue>> radioQueues;
inline void (*radioTask)(void *) = nullptr;
inline void *radioContext = nullptr;
struct WorkerIdle {};
inline std::function<void()> radioOnIdle;
inline QueueHandle_t xQueueCreate(unsigned, unsigned size) {
  radioQueues.emplace_back(new FakeQueue{size,{}}); return radioQueues.back().get();
}
inline int xQueueOverwrite(QueueHandle_t queue, const void *value) {
  auto bytes = static_cast<const uint8_t *>(value); queue->commands.clear();
  queue->commands.emplace_back(bytes, bytes + queue->size); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void *out, TickType_t) {
  if (queue->commands.empty() && radioOnIdle) radioOnIdle();
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
