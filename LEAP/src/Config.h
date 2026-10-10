#pragma once
#include <Arduino.h>
#include "JsonLimits.h"
#if __has_include("../LocalConfig.h")
#include "../LocalConfig.h"
#else
#ifndef LEAP_PROVISION_DEVICE
#define LEAP_PROVISION_DEVICE 0
#endif
#include "../LocalConfig.example.h"
#endif
// Existing cable-installation LocalConfig.h files keep working on migration.
#ifndef LEAP_PROVISION_DEVICE
#define LEAP_PROVISION_DEVICE 1
#endif
#ifndef LEAP_CONFIG_REVISION
#define LEAP_CONFIG_REVISION 1
#endif
#ifndef LEAP_LEFT_KEYS
#define LEAP_LEFT_KEYS {4, 5, 6, 7, 8}
#endif
#ifndef LEAP_RIGHT_KEYS
#define LEAP_RIGHT_KEYS {2, 15, 16, 21, 47}
#endif
#ifndef LEAP_IMU_SWAP_XY
#define LEAP_IMU_SWAP_XY 0
#endif
#ifndef LEAP_IMU_X_SIGN
#define LEAP_IMU_X_SIGN 1
#endif
#ifndef LEAP_IMU_Y_SIGN
#define LEAP_IMU_Y_SIGN 1
#endif
#ifndef LEAP_IMU_Z_SIGN
#define LEAP_IMU_Z_SIGN 1
#endif
static_assert(LEAP_PROVISION_DEVICE == 0 || LEAP_PROVISION_DEVICE == 1,
              "Provisioning mode must be 0 or 1");
#include "DeviceConfig.h"
namespace leap {
constexpr char FirmwareVersion[] = "1.0.3";
constexpr size_t ReserveBytes = 128 * 1024;
constexpr uint32_t SyncInterval = 15 * 60 * 1000, HttpTimeout = 5000;
constexpr size_t MaxPackages = 64, MaxFiles = 256;
inline void log(const char *area, const char *message) {
  Serial.printf("[%10lu] [%-8s] %s\n", millis(), area, message);
}
} // namespace leap
