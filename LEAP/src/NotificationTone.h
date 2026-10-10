#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace leap {
// Short decaying chime mixed into music/game audio, never queued behind a WAV.
class NotificationTone {
  double seconds = 1;
public:
  void start() { seconds = 0; }
  bool active() const { return seconds < 0.18; }
  int16_t next(uint32_t rate) {
    if (!active() || !rate) return 0;
    double envelope = 1 - seconds / 0.18;
    int16_t sample = std::lround(std::sin(seconds * 1200 * 6.283185307179586) * 6500 * envelope * envelope);
    seconds += 1.0 / rate;
    return sample;
  }
  static int16_t mix(int16_t sound, int16_t chime) {
    return std::clamp(int(sound) + int(chime), -32768, 32767);
  }
};
} // namespace leap
