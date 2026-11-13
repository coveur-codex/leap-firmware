#include "IdlePolicy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace leap;
int main() {
  RadioIdle timer;
  assert(!timer.expired(RadioIdle::TimeoutMs));
  timer.show(true, 1000);
  for (uint32_t dt : {60000u, 180000u, 1799999u}) {
    timer.show(true, 1000 + dt); // frames, config reloads and notifications do not reset it
    assert(!timer.expired(1000 + dt));
    auto policy = IdlePolicy::evaluate(1000 + dt, 1000, false, false, true);
    assert(policy.dim && !policy.lock && !policy.openNotification);
  }
  assert(timer.expired(1000 + 1800000));
  timer.input(1000 + 1799999); // station/volume input grants another full 30 minutes
  assert(!timer.expired(1000 + 1800000));
  assert(!timer.expired(1000 + 1799999 + 1799999));
  assert(timer.expired(1000 + 1799999 + 1800000));
  timer.show(false, 4000000);
  assert(!timer.expired(6000000));
  timer.input(6000000); // unrelated page input cannot reactivate the timer
  assert(!timer.expired(8000000));
  timer.show(true, 8000000);
  assert(!timer.expired(8000000 + 1799999));
  assert(timer.expired(8000000 + 1800000));
  uint32_t enter = UINT32_MAX - 5000;
  timer.show(false, enter);
  timer.show(true, enter);
  assert(!timer.expired(enter + 1799999u));
  assert(timer.expired(enter + 1800000u));
  auto beforeDim = IdlePolicy::evaluate(59999, 0, false, false);
  assert(!beforeDim.dim && !beforeDim.lock && !beforeDim.openNotification);
  auto dimOnly = IdlePolicy::evaluate(60000, 0, false, false);
  assert(dimOnly.dim && !dimOnly.lock && dimOnly.openNotification);
  auto normal = IdlePolicy::evaluate(180000, 0, false, false);
  assert(normal.dim && normal.lock && normal.openNotification);
  auto chill = IdlePolicy::evaluate(1800000, 0, true, false);
  assert(!chill.dim && !chill.lock && !chill.openNotification);
  auto diagnostics = IdlePolicy::evaluate(1800000, 0, false, true);
  assert(!diagnostics.dim && !diagnostics.lock && !diagnostics.openNotification);
  puts("PASS: radio idle 30-minute boundary, genuine input, reentry, notification immunity and millis rollover; ordinary idle/Chill/diagnostics preserved");
}
