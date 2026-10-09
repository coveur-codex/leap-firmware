#include "Config.h"
#include "Motion.h"
#include <Wire.h>
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
  Wire.fail = true; fakeNow = 60; motion.poll();
  assert(!motion.available); // UI must show missing MPU instead of stale readings.
  puts("PASS: MPU raw scaling, Z-down/swap mounting transform and I2C failure");
}
