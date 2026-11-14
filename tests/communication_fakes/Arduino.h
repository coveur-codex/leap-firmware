#pragma once
#include "../storage_fakes/Arduino.h"
#include "../compat/Strlcpy.h"
inline unsigned uxQueueSpacesAvailable(QueueHandle_t q) { return q->capacity - q->items.size(); }
