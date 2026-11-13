#include "Audio.h"
#include "Radio.h"
#include <driver/i2s_std.h>
#include <cassert>
using namespace leap;
// Deterministic PCM backend. This test executes the ESP32 branch of the real
// Audio.cpp, including its shared I2S ownership and cancellation fence.
namespace leap {
Radio radio;
int cancellation = 0;
void Radio::play(const String &) {
  requestedEpoch = generation.fetch_add(1) + 1;
}
void Radio::stop() {
  requestedEpoch = 0;
  generation.fetch_add(1);
}
size_t Radio::read(int16_t *out, size_t frames, uint32_t &rate) {
  rate = 44100;
  for (size_t i = 0; i < frames * 2; ++i) out[i] = 1000;
  if (cancellation == 1) stop();
  if (cancellation == 2) play("replacement");
  return frames;
}
}
struct Complete {};
int main() {
  for (int mode : {0, 1, 2}) {
    assert(audio.begin());
    audio.volume = 35;
    cancellation = mode;
    radio.play("first");
    unsigned blocks = 0;
    bool mixed = false, normalSound = false;
    fakeAudioWrite = [&](const int16_t *samples, size_t count) {
      ++blocks;
      if (mode) {
        // A leave or replacement while copying PCM must not leak the old frame.
        for (size_t i = 0; i < count; ++i) assert(samples[i] == 0);
        throw Complete{};
      }
      if (blocks == 1) {
        for (size_t i = 0; i < count; ++i) assert(samples[i] == 350);
        audio.notify(NotificationSound::Pling);
      } else if (blocks == 2) {
        for (size_t i = 0; i < count; ++i) if (samples[i] != 350) mixed = true;
        radio.stop();
        audio.tone(660, 200);
      } else if (blocks == 3) {
        // Original game/system sound path is usable immediately after radio.
        for (size_t i = 0; i < count; ++i) if (samples[i]) normalSound = true;
        throw Complete{};
      }
    };
    try { fakeTask(fakeContext); } catch (const Complete &) {}
    if (!mode) assert(blocks == 3 && mixed && normalSound);
    else assert(blocks == 1);
  }
  puts("PASS: actual radio I2S branch scales PCM, mixes notifications, fences leave/switch and resumes system sounds");
}
