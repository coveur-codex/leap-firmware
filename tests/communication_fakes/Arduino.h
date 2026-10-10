#pragma once
#include "../storage_fakes/Arduino.h"
inline size_t strlcpy(char *dest, const char *src, size_t n) {
  size_t size = strlen(src);
  if (n) { memcpy(dest, src, std::min(size, n-1)); dest[std::min(size,n-1)] = 0; }
  return size;
}
inline unsigned uxQueueSpacesAvailable(QueueHandle_t q) { return q->capacity - q->items.size(); }
