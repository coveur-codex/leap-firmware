#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace leap {
// Skip only declared ID3v2 metadata at the beginning of a file. Never allocate it
// or feed embedded artwork (which may contain MP3-like bytes) to the decoder.
class Mp3Metadata {
public:
  static constexpr uint32_t MaxBytes=16*1024*1024;
  enum class State { Ready, NeedMore, Skipped, Invalid };
  struct Result { State state; size_t consumed=0; uint32_t tagBytes=0; uint8_t version=0; };
private:
  bool leading;
  uint32_t remaining=0, declared=0;
public:
  explicit Mp3Metadata(bool fromBeginning=true):leading(fromBeginning) {}
  bool pending() const { return remaining!=0; }
  uint32_t declaredBytes() const { return declared; }
  Result consume(const uint8_t *data,size_t size) {
    if (!leading) return {State::Ready};
    uint32_t tagBytes=0; uint8_t version=0;
    if (!remaining) {
      if (memcmp(data,"ID3",std::min(size,size_t(3)))) { leading=false; return {State::Ready}; }
      if (size<3) return {State::NeedMore};
      if (size<10) return {State::NeedMore};
      version=data[3];
      if (version<2 || version>4 || data[4]==0xff) return {State::Invalid};
      uint8_t reserved=version==2 ? 0x3f : version==3 ? 0x1f : 0x0f;
      if (data[5]&reserved) return {State::Invalid};
      uint32_t payload=0;
      for (int i=6;i<10;++i) {
        if (data[i]&0x80) return {State::Invalid};
        payload=(payload<<7)|data[i];
      }
      tagBytes=10+payload+(version==4 && (data[5]&0x10) ? 10 : 0);
      if (tagBytes>MaxBytes-declared) return {State::Invalid};
      declared+=tagBytes;
      remaining=tagBytes;
    }
    size_t count=std::min(size,size_t(remaining));
    remaining-=uint32_t(count);
    return {State::Skipped,count,tagBytes,version};
  }
};
} // namespace leap
