#include "TimeModule.h"
#include "Notifications.h"
#include "NotificationTone.h"
#include <cassert>
#include <climits>
using namespace leap;
int main() {
  TimeModule time;
  time.setSeconds(3); time.toggleTimer(UINT32_MAX - 999);
  time.toggleStopwatch(UINT32_MAX - 999);
  assert(!time.tick(0)); assert(time.secondsLeft() == 2);
  time.lap(500); assert(time.laps[0] == 1500);
  time.toggleTimer(500); assert(!time.running());
  time.tick(1500); assert(time.secondsLeft() == 2);
  time.toggleTimer(1500);
  assert(time.tick(3000)); assert(time.done() && time.secondsLeft() == 0);
  assert(!time.tick(9000)); // exactly one notification, despite delayed background ticks
  assert(time.elapsedMs() == 10000);
  time.toggleStopwatch(9000); time.tick(10000); assert(time.elapsedMs() == 10000);
  time.resetStopwatch(); assert(time.elapsedMs() == 0 && time.lapCount == 0);
  time.toggleTimer(10000); assert(time.running() && time.secondsLeft() == 3);
  time.resetTimer(); assert(!time.running() && time.secondsLeft() == 3);
  time.setSeconds(0); assert(time.secondsLeft() == 1);
  time.setSeconds(UINT32_MAX); assert(time.secondsLeft() == TimeModule::MaxSeconds);
  time.setSeconds(1); time.toggleTimer(11000); time.toggleTimer(12000);
  assert(time.tick(12000)); // input exactly at expiry cannot swallow the alarm
  for (unsigned i = 0; i < 25; ++i) { time.toggleStopwatch(13000 + i * 2); time.lap(13001 + i * 2); }
  assert(time.lapCount <= 20);
  Notifications notices;
  notices.submit({"news", "News", "", ""}); notices.tick(0);
  notices.submit({"time", "Fertig", "", "", 0x1234, NotificationSound::Alarm});
  assert(notices.tick(1)); assert(notices.visible()->service == "time");
  assert(notices.visible()->color == 0x1234 && notices.unread("time"));
  NotificationTone tone;
  tone.start(NotificationSound::Alarm);
  int nonzero = 0, samples = 0;
  while (tone.active()) { if (tone.next(8000)) ++nonzero; ++samples; }
  assert(samples >= 9600 && samples <= 9601 && nonzero > 3000);
  tone.start(NotificationSound::Silent); assert(!tone.active());
}
