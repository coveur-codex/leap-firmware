#pragma once
#include "../game_fakes/Arduino.h"
inline void vTaskDelete(void *) { throw SamplingFinished{}; }
