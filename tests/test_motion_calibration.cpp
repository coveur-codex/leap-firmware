#include "MotionCalibration.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
using namespace leap;
static void capture(MotionCalibration &c, const float *v) {
  c.capture();
  for (int i = 0; i < 49; ++i) assert(!c.sample(v[0],v[1],v[2],0,0,0));
  assert(c.sample(v[0],v[1],v[2],0,0,0));
}
int main() {
  int permutation[]{0,1,2};
  do {
    for (int signs = 0; signs < 8; ++signs) {
      float basis[3][3]{};
      for (int i = 0; i < 3; ++i) basis[i][permutation[i]] = signs & (1<<i) ? -1 : 1;
      MotionCalibration c;
      capture(c, basis[2]); capture(c, basis[0]); capture(c, basis[1]);
      assert(c.step == 3 && c.result.valid());
      for (int i = 0; i < 3; ++i) {
        float x = basis[i][0], y = basis[i][1], z = basis[i][2];
        c.result.orient(x,y,z);
        assert(x == (i==0) && y == (i==1) && z == (i==2));
      }
      c.result.axes[0][0] += 0.5f;
      assert(!c.result.valid());
    }
  } while (std::next_permutation(permutation, permutation+3));
  MotionCalibration c;
  float z[]{0,0,1}, x[]{0.8f,0.6f,0}, y[]{-0.6f,0.8f,0};
  capture(c,z);
  c.capture();
  for (int i = 0; i < 50; ++i) c.sample(0,0,1,0,0,0);
  assert(c.step == 1 && c.rejected && !c.collecting); // Duplicate pose.
  c.capture();
  for (int i = 0; i < 100; ++i) {
    c.sample(0,0,0,0,0,0); // Free fall.
    c.sample(0,0,2,0,0,0); // Acceleration/saturation.
    c.sample(1,0,0,10,0,0); // Moving/rotating.
    c.sample(std::numeric_limits<float>::quiet_NaN(),0,0,0,0,0);
  }
  assert(c.step == 1 && c.collecting);
  // A change midway resets the consecutive stability window.
  for (int i = 0; i < 30; ++i) c.sample(1,0,0,0,0,0);
  for (int i = 0; i < 30; ++i) c.sample(0,1,0,0,0,0);
  assert(c.step == 1);
  capture(c,x); capture(c,y);
  assert(c.result.valid());
  float a=0.8f,b=0.6f,d=0;
  c.result.orient(a,b,d);
  assert(std::fabs(a-1) < 0.001f && std::fabs(b) < 0.001f && d == 0);
  c.result.axes[0][0] = 0;
  c.result.checksum = c.result.hash();
  assert(!c.result.valid()); // Semantically invalid even with correct checksum.
  puts("PASS: 48 axis mountings, arbitrary rotation, stationary capture, bad/duplicate poses and record validation");
}
