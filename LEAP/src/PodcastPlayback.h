#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
namespace leap {
// Positions are decoded sample counts, never an estimate from average MP3 bitrate.
struct Mp3SeekIndex {
  struct Point { uint32_t offset = 0; uint64_t samples = 0; };
  Point points[512]{};
  uint16_t size = 0;
  uint32_t rate = 0;
  void add(uint64_t offset, uint64_t samples, uint32_t hz) {
    if (!hz || offset > UINT32_MAX || size == 512) return;
    rate = hz;
    if (!size || samples >= points[size-1].samples + uint64_t(hz)*30)
      points[size++] = {uint32_t(offset), samples};
  }
  Point before(uint32_t seconds) const {
    Point result;
    // Two seconds of preroll restores minimp3's bit reservoir after a byte seek.
    uint64_t target = uint64_t(seconds > 2 ? seconds-2 : 0)*rate;
    for (size_t i=0; i<size && points[i].samples<=target; ++i) result=points[i];
    return result;
  }
};
inline uint32_t podcastSeek(uint32_t position, int delta, uint32_t duration) {
  int64_t target = int64_t(position) + delta;
  if (duration) target = std::min(target, int64_t(duration));
  return uint32_t(std::max(int64_t(0), std::min(target, int64_t(7*86400))));
}
// A seek into the final seconds must never count as listening to the whole episode.
inline bool podcastNaturalEnd(bool explicitSeek, uint32_t secondsAfterSeek) {
  return !explicitSeek || secondsAfterSeek >= 5;
}
} // namespace leap
