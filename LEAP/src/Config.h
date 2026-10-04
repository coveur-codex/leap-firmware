#pragma once
#include <Arduino.h>
#include "JsonLimits.h"
#if __has_include("../LocalConfig.h")
#include "../LocalConfig.h"
#else
#include "../LocalConfig.example.h"
#endif
namespace leap {
constexpr char FirmwareVersion[] = "1.0.0-beta.16";
constexpr size_t ReserveBytes = 128 * 1024;
constexpr uint32_t SyncInterval = 15 * 60 * 1000, HttpTimeout = 5000;
constexpr size_t MaxPackages = 64, MaxFiles = 256;
inline void log(const char *area, const char *message) {
  Serial.printf("[%10lu] [%-8s] %s\n", millis(), area, message);
}
} // namespace leap
