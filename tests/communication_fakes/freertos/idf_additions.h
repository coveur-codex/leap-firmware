#pragma once
#include <Arduino.h>
inline size_t fakePsramQueueBytes = 0;
inline unsigned fakePsramQueueCaps = 0;
inline unsigned fakePsramQueueDeleted = 0;
inline QueueHandle_t xQueueCreateWithCaps(unsigned count, unsigned size, unsigned caps) {
  fakePsramQueueBytes = count * size; fakePsramQueueCaps = caps;
  return xQueueCreate(count, size);
}
inline void vQueueDeleteWithCaps(QueueHandle_t queue) {
  ++fakePsramQueueDeleted; vQueueDelete(queue);
}
