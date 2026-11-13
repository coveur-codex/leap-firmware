#pragma once
#include <Arduino.h>
inline void xQueueReset(QueueHandle_t q) { q->items.clear(); }
