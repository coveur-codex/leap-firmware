#include "NotificationTone.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace leap;
int main() {
  for (uint32_t rate : {8000,22050,48000}) {
    NotificationTone tone;
    assert(!tone.active() && tone.next(rate)==0);
    tone.start(); int samples=0, loudest=0; double energy=0;
    while (tone.active()) {
      int sample=tone.next(rate); loudest=std::max(loudest,std::abs(sample)); energy+=sample*sample;
      assert(NotificationTone::mix(32000, sample) <= 32767);
      ++samples;
    }
    assert(std::abs(samples-int(rate*0.18))<=1 && loudest>3000 && energy>1000000);
    assert(tone.next(rate)==0);
    tone.start(); assert(tone.active()); // A new notification restarts promptly.
  }
  assert(NotificationTone::mix(32000,5000)==32767);
  assert(NotificationTone::mix(-32000,-5000)==-32768);
  puts("PASS: decaying chime at WAV sample rates, immediate restart, mixing and saturation");
}
