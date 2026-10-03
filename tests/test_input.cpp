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

  // Failure to start a task must preserve the original direct-polling fallback.
  Input fallback;
  fallback.begin();
  fakeTaskFailure = true;
  assert(!fallback.start());
  fakeNow = 0;
  assert(!fallback.poll(event));
  fakeNow = 25;
  assert(fallback.poll(event) && event.key == Key::Up && !event.longPress);
  puts("PASS: short simultaneous presses during UI stall, bounded queue and polling fallback");
}
