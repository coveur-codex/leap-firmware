#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace leap {
// Schema 1 is a fixed, little-endian NVS blob. Keep its layout stable; future
// schemas must explicitly migrate this record rather than reseed OTA defaults.
struct DeviceSettings {
  uint32_t schema = 1, revision = 0;
  char deviceId[81]{}, ssid[33]{}, password[65]{}, server[257]{}, timezone[129]{}, tlsCa[4097]{};
  uint8_t leftKeys[5]{4, 5, 6, 7, 8}, rightKeys[5]{2, 15, 16, 21, 47};
  uint8_t radioChannel = 6, otaEnabled = 1, swapXY = 0;
  int8_t xSign = 1, ySign = 1, zSign = 1;
  uint32_t checksum = 0;

  uint32_t hash() const {
    uint32_t value = 2166136261u;
    const auto *bytes = reinterpret_cast<const uint8_t *>(this);
    for (size_t i = 0; i < offsetof(DeviceSettings, checksum); ++i)
      value = (value ^ bytes[i]) * 16777619u;
    return value;
  }
  static bool inputPin(uint8_t pin) {
    // Exclude display, I2C, I2S, USB, flash/PSRAM, and input-only GPIO46
    // (the switches need internal pull-ups).
    return (pin >= 1 && pin <= 8) || pin == 15 || pin == 16 || pin == 21 || pin == 38 ||
           (pin >= 42 && pin <= 48 && pin != 46);
  }
  bool valid() const {
    if (schema != 1 || !revision || checksum != hash() || !deviceId[0] || !server[0] ||
        !timezone[0] || radioChannel < 1 || radioChannel > 14 || otaEnabled > 1 || swapXY > 1)
      return false;
    if (!memchr(deviceId, 0, sizeof(deviceId)) || !memchr(ssid, 0, sizeof(ssid)) ||
        !memchr(password, 0, sizeof(password)) || !memchr(server, 0, sizeof(server)) ||
        !memchr(timezone, 0, sizeof(timezone)) || !memchr(tlsCa, 0, sizeof(tlsCa)))
      return false;
    if ((xSign != 1 && xSign != -1) || (ySign != 1 && ySign != -1) ||
        (zSign != 1 && zSign != -1) || xSign * ySign * zSign != (swapXY ? -1 : 1))
      return false; // A physical mounting is a rotation, not a reflection.
    uint64_t used = 0;
    for (int i = 0; i < 10; ++i) {
      uint8_t pin = i < 5 ? leftKeys[i] : rightKeys[i - 5];
      if (!inputPin(pin) || (used & (uint64_t(1) << pin)))
        return false;
      used |= uint64_t(1) << pin;
    }
    return true;
  }
  void orient(float &x, float &y, float &z) const {
    float oldX = x;
    x = (swapXY ? y : x) * xSign;
    y = (swapXY ? oldX : y) * ySign;
    z *= zSign;
  }
};
static_assert(sizeof(DeviceSettings) == 4692, "NVS schema layout changed");
} // namespace leap
