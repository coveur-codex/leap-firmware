#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Notifications.h"
namespace leap {
// Short decaying chime mixed into music/game audio, never queued behind a WAV.
class NotificationTone {
  double seconds = 1;
  NotificationSound sound = NotificationSound::Pling;
  uint32_t noise = 0x9e3779b9;
  double duration() const { return sound == NotificationSound::Swish ? 0.3 : 0.18; }
public:
  void start(NotificationSound kind = NotificationSound::Pling) { sound = kind; seconds = kind == NotificationSound::Silent ? 1 : 0; }
  bool active() const { return seconds < duration(); }
  int16_t next(uint32_t rate) {
    if (!active() || !rate) return 0;
    double envelope = 1 - seconds / duration();
    double wave = std::sin(seconds * (sound == NotificationSound::Plong ? 600 : 1200) * 6.283185307179586);
    if (sound == NotificationSound::Swish) {
      noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
      wave = (int32_t(noise & 65535) - 32768) / 32768.0;
      envelope *= std::min(1.0, seconds / 0.015);
    }
    int16_t sample = std::lround(wave * 6500 * envelope * envelope);
    seconds += 1.0 / rate;
    return sample;
  }
  static int16_t mix(int16_t sound, int16_t chime) {
    return std::clamp(int(sound) + int(chime), -32768, 32767);
  }
};
} // namespace leap
