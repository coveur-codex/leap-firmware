#include "Config.h"
#include <limits.h>

namespace leap {
DeviceSettings deviceSettings;

#if LEAP_PROVISION_DEVICE
  static_assert(LEAP_RADIO_CHANNEL >= 1 && LEAP_RADIO_CHANNEL <= 14, "Invalid radio channel");
  static_assert(LEAP_OTA_ENABLED == 0 || LEAP_OTA_ENABLED == 1, "OTA flag must be 0 or 1");
  static_assert(LEAP_IMU_SWAP_XY == 0 || LEAP_IMU_SWAP_XY == 1, "Axis swap must be 0 or 1");
  static_assert((LEAP_IMU_X_SIGN == 1 || LEAP_IMU_X_SIGN == -1) &&
                (LEAP_IMU_Y_SIGN == 1 || LEAP_IMU_Y_SIGN == -1) &&
                (LEAP_IMU_Z_SIGN == 1 || LEAP_IMU_Z_SIGN == -1), "Axis signs must be +1 or -1");
// Referenced at boot so the build marker survives linker garbage collection.
static constexpr char BuildKind[] = "LEAP_DEVICE_PROVISIONING_V1";
template <size_t N> bool copySetting(char (&out)[N], const char *value) {
  size_t length = strlen(value);
  if (length >= N)
    return false;
  memcpy(out, value, length + 1);
  return true;
}
#else
static constexpr char BuildKind[] = "LEAP_UNIVERSAL_NVS_V1";
#endif

bool beginDeviceConfig() {
  log("CONFIG", BuildKind);
  Preferences prefs;
#if LEAP_PROVISION_DEVICE
  std::unique_ptr<DeviceSettings> seedStorage(new (std::nothrow) DeviceSettings{});
  if (!seedStorage) {
    prefs.end();
    return false;
  }
  auto &seed = *seedStorage;
  static_assert(LEAP_CONFIG_REVISION > 0 && LEAP_CONFIG_REVISION <= UINT_MAX,
                "Configuration revision must be a positive uint32");
  seed.revision = LEAP_CONFIG_REVISION;
  bool copied = copySetting(seed.deviceId, LEAP_DEVICE_ID) && copySetting(seed.ssid, LEAP_WIFI_SSID) &&
                copySetting(seed.password, LEAP_WIFI_PASSWORD) && copySetting(seed.server, LEAP_SERVER) &&
                copySetting(seed.timezone, LEAP_TIMEZONE) && copySetting(seed.tlsCa, LEAP_TLS_CA);
  const uint8_t left[] = LEAP_LEFT_KEYS, right[] = LEAP_RIGHT_KEYS;
  static_assert(sizeof(left) == 5 && sizeof(right) == 5, "Each switch requires five logical pins");
  memcpy(seed.leftKeys, left, 5);
  memcpy(seed.rightKeys, right, 5);
  seed.radioChannel = LEAP_RADIO_CHANNEL;
  seed.otaEnabled = LEAP_OTA_ENABLED;
  seed.swapXY = LEAP_IMU_SWAP_XY;
  seed.xSign = LEAP_IMU_X_SIGN;
  seed.ySign = LEAP_IMU_Y_SIGN;
  seed.zSign = LEAP_IMU_Z_SIGN;
  seed.checksum = seed.hash();
  bool ok = copied && loadDeviceSettings(prefs, deviceSettings, &seed);
#else
  bool ok = loadDeviceSettings(prefs, deviceSettings);
#endif
  prefs.end();
  if (ok)
    log("CONFIG", "Device configuration loaded from NVS");
  return ok;
}
} // namespace leap
