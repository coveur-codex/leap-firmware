#pragma once
#define xQueueReceive baseQueueReceive
#include "../game_fakes/Arduino.h"
#undef xQueueReceive
struct FakeQueueDrained {};
inline int xQueueReceive(QueueHandle_t q, void *item, TickType_t wait) {
  if (q->items.empty())
    throw FakeQueueDrained{};
  return baseQueueReceive(q, item, wait);
}
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  return reinterpret_cast<void *>(1);
}
inline void xSemaphoreTake(SemaphoreHandle_t, TickType_t) {}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline void vTaskDelay(unsigned) {}
