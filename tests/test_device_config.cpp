#include "DeviceConfig.h"
#include <cassert>
#include <cstdio>
using namespace leap;
DeviceSettings seed(const char *id, uint32_t revision = 1) {
  DeviceSettings s{};
  strcpy(s.deviceId, id);
  strcpy(s.server, "http://192.168.1.10:8000");
  strcpy(s.timezone, "UTC0");
  s.revision = revision;
  s.checksum = s.hash();
  return s;
}
int main() {
  Preferences prefs;
  DeviceSettings loaded{};
  assert(!loadDeviceSettings(prefs, loaded)); // OTA on an unprovisioned device.
  auto a = seed("leap-a");
  assert(loadDeviceSettings(prefs, loaded, &a));
  assert(loaded.valid() && !strcmp(loaded.deviceId, "leap-a"));
  unsigned writes = Preferences::writes["leap-deviceconfig"];
  auto b = seed("leap-b");
  // Neither OTA nor repeated provisioning at the same revision overwrites A.
  assert(loadDeviceSettings(prefs, loaded));
  assert(loadDeviceSettings(prefs, loaded, &b));
  assert(!strcmp(loaded.deviceId, "leap-a"));
  assert(Preferences::writes["leap-deviceconfig"] == writes);
  b.revision = 2;
  b.leftKeys[0] = 47; b.rightKeys[4] = 4; // Exchanged / rotated switches.
  b.ySign = -1; b.zSign = -1; // Mounted upside down around X.
  b.checksum = b.hash();
  Preferences::failWrites = true;
  assert(!loadDeviceSettings(prefs, loaded, &b));
  Preferences::failWrites = false;
  assert(loadDeviceSettings(prefs, loaded));
  assert(!strcmp(loaded.deviceId, "leap-a"));
  assert(loadDeviceSettings(prefs, loaded, &b));
  assert(loadDeviceSettings(prefs, loaded));
  assert(!strcmp(loaded.deviceId, "leap-b") && loaded.leftKeys[0] == 47);
  float x = 0.1f, y = 0.2f, z = -1;
  loaded.orient(x, y, z);
  assert(x == 0.1f && y == -0.2f && z == 1);
  assert(loadDeviceSettings(prefs, loaded, &a)); // Older USB image cannot revert settings.
  assert(loaded.revision == 2);
  auto invalid = b;
  invalid.leftKeys[0] = invalid.leftKeys[1]; invalid.checksum = invalid.hash();
  assert(!invalid.valid());
  invalid = b; invalid.rightKeys[0] = 19; invalid.checksum = invalid.hash();
  assert(!invalid.valid()); // USB/reserved GPIO.
  invalid = b; invalid.swapXY = 1; invalid.checksum = invalid.hash();
  assert(!invalid.valid()); // Reflection is not a physical mounting.
  invalid = b; memset(invalid.deviceId, 'x', sizeof(invalid.deviceId));
  invalid.checksum = invalid.hash(); assert(!invalid.valid());
  invalid = b; invalid.schema = 2; invalid.checksum = invalid.hash();
  assert(!invalid.valid());
  // Exercise all eight valid horizontal mounting rotations (accel + gyro).
  unsigned orientations = 0;
  for (int swap = 0; swap <= 1; ++swap)
    for (int sx : {-1, 1}) for (int sy : {-1, 1}) for (int sz : {-1, 1}) {
      auto s = a; s.swapXY = swap; s.xSign = sx; s.ySign = sy; s.zSign = sz;
      s.checksum = s.hash();
      if (!s.valid()) continue;
      ++orientations;
      float ax = 2, ay = 3, az = 5;
      s.orient(ax, ay, az);
      assert(ax == (swap ? 3 : 2) * sx && ay == (swap ? 2 : 3) * sy && az == 5 * sz);
    }
  assert(orientations == 8);
  Preferences::bytes["leap-deviceconfig"][100] ^= 1;
  assert(!loadDeviceSettings(prefs, loaded));
  assert(!loadDeviceSettings(prefs, loaded, &b)); // Corruption requires deliberate recovery.
  puts("PASS: NVS provisioning, OTA preservation, revisions, failures and mounting rotations");
}
