#include "Config.h"
#include "Motion.h"
#include <Wire.h>
#include <Preferences.h>
#include <cassert>
#include <cstdio>
namespace leap { DeviceSettings deviceSettings; }
using namespace leap;
int main() {
  assert(motion.begin());
  auto value = [](int index, int16_t n) {
    Wire.sample[index] = uint16_t(n) >> 8;
    Wire.sample[index + 1] = uint16_t(n) & 255;
  };
  value(0, 16384); value(2, 8192); value(4, -16384);
  value(8, 131); value(10, -262); value(12, 393);
  deviceSettings.ySign = -1; deviceSettings.zSign = -1;
  fakeNow = 20; motion.poll();
  assert(motion.rawX == 1 && motion.rawY == 0.5f && motion.rawZ == -1);
  assert(motion.rawGx == 1 && motion.rawGy == -2 && motion.rawGz == 3);
  assert(motion.x == 1 && motion.y == -0.5f && motion.z == 1);
  assert(motion.gx == 1 && motion.gy == 2 && motion.gz == -3);
  deviceSettings.swapXY = 1; deviceSettings.xSign = -1;
  deviceSettings.ySign = 1; deviceSettings.zSign = 1;
  fakeNow = 40; motion.poll();
  assert(motion.x == -0.5f && motion.y == 1 && motion.z == -1);
  assert(motion.gx == 2 && motion.gy == 1 && motion.gz == 3);
  assert(motion.rawY == 0.5f && motion.rawGy == -2); // Raw values remain sensor-relative.
  motion.startCalibration();
  auto capture = [&](int16_t a, int16_t b, int16_t c) {
    value(0,a); value(2,b); value(4,c);
    value(8,0); value(10,0); value(12,0);
    motion.calibration.capture();
    for (int i = 0; i < 50; ++i) { fakeNow += 20; motion.poll(); }
  };
  capture(16384,0,0); // Z is physically sensor X.
  capture(0,-16384,0); // Device right is sensor -Y.
  Preferences::failWrites = true;
  capture(0,0,16384); // Device down is sensor Z.
  assert(motion.calibration.step == 3 && !motion.calibrationSaved);
  assert(motion.x == 0 && motion.y == 0 && motion.z == 1); // Prior config still applies.
  Preferences::failWrites = false;
  assert(motion.saveCalibration());
  assert(motion.begin()); // Simulated restart reloads the committed record.
  value(0,0); value(2,-16384); value(4,0);
  value(8,0); value(10,-131); value(12,0);
  fakeNow += 20; motion.poll();
  assert(motion.x == 1 && motion.y == 0 && motion.z == 0);
  assert(motion.gx == 1 && motion.gy == 0 && motion.gz == 0);
  auto &stored = Preferences::bytes["leap-imuorientation"];
  stored[0] ^= 1;
  assert(motion.begin());
  value(0,16384); value(2,0); value(4,0);
  fakeNow += 20; motion.poll();
  assert(motion.x == 0 && motion.y == 1 && motion.z == 0); // Saved mounting defaults.
  Wire.fail = true; fakeNow += 20; motion.poll();
  assert(!motion.available); // UI must show missing MPU instead of stale readings.
  puts("PASS: MPU raw scaling, Z-down/swap mounting transform calibration persistence/retry/corruption and I2C failure");
}
