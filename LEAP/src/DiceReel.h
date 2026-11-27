#pragma once
#include <cstdint>

namespace leap {
// Align the reel to the confirmed face, then ease to zero speed.
inline uint32_t brakeReel(uint32_t age, uint32_t duration, uint32_t start,
                          uint32_t pitch, uint32_t faces, uint32_t result) {
  uint32_t tile = (start + duration / 8 + pitch - 1) / pitch;
  tile += (faces - (tile + result) % faces) % faces;
  const uint32_t distance = tile * pitch - start;
  return start + uint64_t(distance) * age * (2 * duration - age) / (uint64_t(duration) * duration);
}
} // namespace leap
