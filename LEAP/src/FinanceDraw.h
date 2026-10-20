#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include "Finance.h"
namespace leap {
void drawFinance(Arduino_GFX &display, JsonVariantConst data, FinanceNavigation &nav,
                 bool online, int64_t now);
}
