#include "Input.h"
#include <cassert>
#include <cstdio>
using namespace leap;
int main() {
  Input input;
  input.begin();
  assert(input.start());
  fakeDown = [](int pin, uint32_t now) {
    return (pin == hw::LeftKeys[4] || pin == hw::RightKeys[4]) && now >= 10 && now < 60;
  };
  // The UI does not poll for 100 ms: both 50 ms presses finish during its stall.
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  InputEvent event;
  assert(input.poll(event) && !event.right && event.key == Key::Center && !event.longPress);
  assert(input.poll(event) && event.right && event.key == Key::Center && !event.longPress);
  assert(!input.poll(event));

  Input bounded;
  bounded.begin();
  assert(bounded.start());
  fakeNow = 0;
  fakeUntil = 6000;
  fakeDown = [](int pin, uint32_t) { return pin == hw::LeftKeys[0]; };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  unsigned count = 0, longPresses = 0;
  while (bounded.poll(event)) {
    assert(!event.right && event.key == Key::Up);
    if (count == 0) assert(!event.longPress);
    longPresses += event.longPress;
    ++count;
  }
  assert(count == 32 && longPresses == 1);

  // Centre holds retain the existing 900 ms event and add exactly one 2 s event.
  Input centres;
  centres.begin();
  assert(centres.start());
  fakeNow = 0;
  fakeUntil = 3000;
  fakeDown = [](int pin, uint32_t now) {
    return (pin == hw::LeftKeys[4] || pin == hw::RightKeys[4]) && now >= 10 && now < 2500;
  };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  unsigned shortCount = 0, regularCount = 0, extendedCount = 0;
  while (centres.poll(event)) {
    assert(event.key == Key::Center);
    if (!event.longPress) { assert(event.heldMs == 0); ++shortCount; }
    else if (event.heldMs == Debouncer::LongPressMs) ++regularCount;
    else { assert(event.heldMs == Input::ExtendedCenterHoldMs); ++extendedCount; }
  }
  assert(shortCount == 2 && regularCount == 2 && extendedCount == 2);
  Debouncer released;
  assert(!released.poll(true, 0, false, Input::ExtendedCenterHoldMs));
  assert(released.poll(true, 25, false, Input::ExtendedCenterHoldMs) == 1);
  assert(released.poll(true, 925, false, Input::ExtendedCenterHoldMs) == 3);
  assert(!released.poll(true, 2024, false, Input::ExtendedCenterHoldMs));
  // Raw release at the threshold must not fabricate a hold before debounce finishes.
  assert(!released.poll(false, 2025, false, Input::ExtendedCenterHoldMs));
  assert(!released.poll(false, 2050, false, Input::ExtendedCenterHoldMs));
  assert(!released.stable);

  // Held direction state survives a full event queue and clears after release.
  Input held;
  held.begin(); assert(held.start());
  fakeNow = 0; fakeUntil = 6000;
  fakeDown = [](int pin, uint32_t) { return pin == hw::RightKeys[3]; };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  assert(held.heldRightDirections() == 8);
  fakeUntil = fakeNow + 100;
  fakeDown = [](int, uint32_t) { return false; };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  assert(held.heldRightDirections() == 0);

  // Failure to start a task must preserve the original direct-polling fallback.
  fakeDown = [](int pin, uint32_t) { return pin == hw::LeftKeys[0]; };
  Input fallback;
  fallback.begin();
  fakeTaskFailure = true;
  assert(!fallback.start());
  fakeNow = 0;
  assert(!fallback.poll(event));
  fakeNow = 25;
  assert(fallback.poll(event) && event.key == Key::Up && !event.longPress);
  Input heldFallback;
  heldFallback.begin();
  assert(!heldFallback.start());
  fakeDown = [](int pin, uint32_t) { return pin == hw::RightKeys[3]; };
  fakeNow = 0;
  assert(!heldFallback.poll(event));
  fakeNow = 25;
  assert(heldFallback.poll(event));
  assert(heldFallback.heldRightDirections() == 8);
  fakeDown = [](int, uint32_t) { return false; };
  fakeNow = 30;
  assert(!heldFallback.poll(event));
  fakeNow = 55;
  assert(!heldFallback.poll(event));
  assert(heldFallback.heldRightDirections() == 0);
  Input centreFallback;
  centreFallback.begin();
  assert(!centreFallback.start());
  fakeDown = [](int pin, uint32_t) { return pin == hw::LeftKeys[4]; };
  fakeNow = 0;
  assert(!centreFallback.poll(event));
  fakeNow = 25;
  assert(centreFallback.poll(event) && !event.longPress);
  fakeNow = 925;
  assert(centreFallback.poll(event) && event.heldMs == Debouncer::LongPressMs);
  fakeNow = 2024;
  assert(!centreFallback.poll(event));
  fakeNow = 2025;
  assert(centreFallback.poll(event) && event.heldMs == Input::ExtendedCenterHoldMs);
  fakeNow = 3025;
  assert(!centreFallback.poll(event));
  // Runtime pin arrays must control both task sampling and polling fallback.
  const uint8_t left[] = {47, 21, 16, 15, 2}, right[] = {8, 7, 6, 5, 4};
  Input mapped;
  fakeTaskFailure = false;
  mapped.begin(left, right); assert(mapped.start());
  fakeNow = 0; fakeUntil = 100;
  fakeDown = [](int pin, uint32_t now) { return pin == 47 && now >= 10 && now < 60; };
  try { fakeTask(fakeContext); } catch (SamplingFinished &) {}
  assert(mapped.poll(event) && !event.right && event.key == Key::Up);
  assert(!mapped.poll(event));
  Input mappedFallback;
  mappedFallback.begin(left, right); fakeTaskFailure = true;
  assert(!mappedFallback.start());
  fakeDown = [](int pin, uint32_t) { return pin == 4; };
  fakeNow = 0; assert(!mappedFallback.poll(event));
  fakeNow = 25;
  assert(mappedFallback.poll(event) && event.right && event.key == Key::Center);
  puts("PASS: short simultaneous presses during UI stall, bounded queue, 900 ms/2 s centre holds "
       "and polling fallback");
}
