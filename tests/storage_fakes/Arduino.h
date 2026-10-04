#pragma once
#include "../game_fakes/Arduino.h"
#include <functional>
inline bool fakeMutexTaken = false;
inline std::function<void()> fakeWriteHook;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
  if (fakeMutexTaken) return 0;
  fakeMutexTaken = true; return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t) { fakeMutexTaken = false; }
inline void vTaskDelay(unsigned) {}
inline void delay(unsigned) {}
