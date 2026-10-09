#include "Config.h"
#include <cassert>
#include <cstdio>
using namespace leap;
int main() {
#if LEAP_PROVISION_DEVICE
  assert(beginDeviceConfig());
  assert(!strcmp(deviceSettings.deviceId, LEAP_DEVICE_ID));
  unsigned writes = Preferences::writes["leap-deviceconfig"];
  assert(beginDeviceConfig());
  assert(Preferences::writes["leap-deviceconfig"] == writes);
#else
  assert(!beginDeviceConfig());
#endif
  // Simulate cable provisioning of another device, then boot the same image.
  DeviceSettings other{};
  strcpy(other.deviceId, "leap-other");
  strcpy(other.ssid, "other-wifi");
  strcpy(other.server, "http://other-server:8000");
  strcpy(other.timezone, "UTC0");
  other.revision = 5;
  other.leftKeys[0] = 47; other.rightKeys[4] = 4;
  other.ySign = -1; other.zSign = -1;
  other.checksum = other.hash();
  Preferences prefs;
  DeviceSettings out{};
  assert(loadDeviceSettings(prefs, out, &other));
  unsigned savedWrites = Preferences::writes["leap-deviceconfig"];
  assert(beginDeviceConfig());
  assert(!strcmp(deviceSettings.deviceId, "leap-other"));
  assert(!strcmp(deviceSettings.ssid, "other-wifi"));
  assert(deviceSettings.leftKeys[0] == 47 && deviceSettings.zSign == -1);
  assert(Preferences::writes["leap-deviceconfig"] == savedWrites);
  puts("PASS: actual build-mode boot loads the device's saved values without reseeding");
}
