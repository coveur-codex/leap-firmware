#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace leap {
// One worker owns this queue. Storage is allocated explicitly in PSRAM.
class Mp3ReadAhead {
  uint8_t *data;
  size_t capacity, head=0, tail=0, count=0;
public:
  static constexpr size_t Bytes=512*1024;
  static constexpr size_t PrefillBytes=32*1024;
  static constexpr uint32_t PrefillMaxMs=2000;
  Mp3ReadAhead(uint8_t *memory,size_t size=Bytes):data(memory),capacity(size) {}
  size_t size() const { return count; }
  size_t free() const { return capacity-count; }
  void clear() { head=tail=count=0; }
  bool push(uint8_t byte) {
    if (count==capacity) return false;
    data[tail]=byte;
    if (++tail==capacity) tail=0;
    ++count;return true;
  }
  size_t read(uint8_t *out,size_t size) {
    size=std::min(size,count);
    if (!size) return 0;
    size_t first=std::min(size,capacity-head);
    memcpy(out,data+head,first);
    memcpy(out+first,data,size-first);
    head=(head+size)%capacity;
    count-=size;return size;
  }
};
struct Mp3StreamWatch {
  static constexpr uint32_t NetworkIdleMs=8000, ConsumerIdleMs=10000;
  uint32_t starvingAt, consumedAt, lastConsumed=0;
  explicit Mp3StreamWatch(uint32_t now):starvingAt(now),consumedAt(now) {}
  bool networkExpired(uint32_t now,bool buffered,bool paused,bool received) {
    if (buffered || paused || received) starvingAt=now;
    return uint32_t(now-starvingAt)>=NetworkIdleMs;
  }
  bool consumerExpired(uint32_t now,uint32_t consumed,size_t queued,bool ready,bool paused) {
    if (consumed!=lastConsumed || !queued || !ready || paused) consumedAt=now;
    lastConsumed=consumed;
    return uint32_t(now-consumedAt)>=ConsumerIdleMs;
  }
};
} // namespace leap
