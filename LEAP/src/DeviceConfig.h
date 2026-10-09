#pragma once
#include "DeviceSettings.h"
#include <Preferences.h>
#include <memory>
#include <new>

namespace leap {
// One atomic NVS blob, independent of the LittleFS content/recovery partition.
// A universal image never writes it. An installation image writes only when
// absent or when the installer explicitly increments its revision.
inline bool loadDeviceSettings(Preferences &prefs, DeviceSettings &out,
                               const DeviceSettings *seed = nullptr) {
  if (!prefs.begin("leap-device", seed == nullptr))
    return false;
  // The TLS root makes each record ~4.7 KiB; keep it off Arduino's 8-KiB task stack.
  std::unique_ptr<DeviceSettings> candidate(new (std::nothrow) DeviceSettings{});
  if (!candidate)
    return false;
  auto &stored = *candidate;
  size_t size = prefs.getBytesLength("config");
  bool valid = size == sizeof(stored) &&
               prefs.getBytes("config", &stored, sizeof(stored)) == sizeof(stored) && stored.valid();
  // Do not silently replace corrupt or future-schema records with defaults.
  if (size && !valid)
    return false;
  if (seed && (!valid || seed->revision > stored.revision)) {
    if (!seed->valid() || prefs.putBytes("config", seed, sizeof(*seed)) != sizeof(*seed))
      return false;
    // Verify the committed record before using it or starting network tasks.
    if (prefs.getBytes("config", &stored, sizeof(stored)) != sizeof(stored) || !stored.valid())
      return false;
    valid = true;
  }
  if (valid)
    out = stored;
  return valid;
}
extern DeviceSettings deviceSettings;
bool beginDeviceConfig();
} // namespace leap
