#pragma once
#include "ChillMusic.h"
namespace leap {
// Unsigned elapsed times keep scheduling correct across millis() rollover.
class PictureTransition {
public:
  enum class Phase { Still, Preparing, FadeOut, Black, FadeIn };
  Phase phase = Phase::Still;
  uint32_t lastStart = 0, phaseAt = 0, interval = 120000;
  bool swap = false;
  static uint32_t intervalFor(int value) {
    return 300000 - std::clamp(value, 0, 300) * 900;
  }
  void reset(uint32_t now, int value) {
    phase = Phase::Still;
    lastStart = phaseAt = now;
    interval = intervalFor(value);
    swap = false;
  }
  void adjust(int value, uint32_t now) {
    interval = intervalFor(value);
    // Give prefetch a quiet scheduling window when shortening an overdue interval.
    if (phase == Phase::Still && uint32_t(now - lastStart) >= interval - 15000)
      lastStart = now - (interval - 15000);
  }
  bool prefetch(uint32_t now, bool hasImage) const {
    return phase == Phase::Still && (!hasImage || uint32_t(now - lastStart) >= interval - 15000);
  }
  PictureCue tick(uint32_t now, bool ready, bool hasImage) {
    swap = false;
    if (phase == Phase::Still && ready &&
        (!hasImage || uint32_t(now - lastStart) >= interval - 1200)) {
      phase = Phase::Preparing;
      phaseAt = now;
      return PictureCue::Prepare;
    }
    if (phase == Phase::Preparing && uint32_t(now - phaseAt) >= 1200) {
      phase = Phase::FadeOut;
      lastStart = phaseAt = now;
      return PictureCue::FadeOut;
    }
    if (phase == Phase::FadeOut && uint32_t(now - phaseAt) >= 1500) {
      phase = Phase::Black;
      phaseAt = now;
      return PictureCue::Black;
    }
    if (phase == Phase::Black && uint32_t(now - phaseAt) >= 300) {
      swap = true;
      phase = Phase::FadeIn;
      phaseAt = now;
      return PictureCue::FadeIn;
    }
    if (phase == Phase::FadeIn && uint32_t(now - phaseAt) >= 1500) {
      phase = Phase::Still;
      return PictureCue::Complete;
    }
    return PictureCue::None;
  }
  uint8_t brightness(uint32_t now) const {
    uint32_t dt = std::min(uint32_t(now - phaseAt), uint32_t(1500));
    if (phase == Phase::Black)
      return 0;
    if (phase == Phase::FadeOut)
      return uint8_t(255 - dt * 255 / 1500);
    if (phase == Phase::FadeIn)
      return uint8_t(dt * 255 / 1500);
    return 255;
  }
  static uint16_t dim(uint16_t p, uint8_t gain) {
    return (((p >> 11) * gain / 255) << 11) | ((((p >> 5) & 63) * gain / 255) << 5) |
           ((p & 31) * gain / 255);
  }
};
} // namespace leap
