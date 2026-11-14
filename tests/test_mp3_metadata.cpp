#include "Mp3Metadata.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace leap;
std::vector<uint8_t> tag(uint8_t version,uint32_t payload,uint8_t flags=0) {
  return {'I','D','3',version,0,flags,uint8_t((payload>>21)&127),uint8_t((payload>>14)&127),uint8_t((payload>>7)&127),uint8_t(payload&127)};
}
int main() {
  // Real BR CheckPod headers, observed 2026-11-14. No publisher audio/artwork.
  for (auto header: {std::vector<uint8_t>{0x49,0x44,0x33,3,0,0,0,0x0c,0x55,0x23},
                    std::vector<uint8_t>{0x49,0x44,0x33,3,0,0,0,0x0f,0x18,0x3e},
                    std::vector<uint8_t>{0x49,0x44,0x33,3,0,0,0,9,0x7e,0x61}}) {
    Mp3Metadata metadata;
    for (size_t size=1;size<10;++size) assert(metadata.consume(header.data(),size).state==Mp3Metadata::State::NeedMore);
    auto result=metadata.consume(header.data(),header.size());
    assert(result.state==Mp3Metadata::State::Skipped && result.consumed==10 && result.tagBytes>128*1024);
    uint32_t skipped=10;
    const uint8_t ignored[1024]{};
    while (metadata.pending()) { auto part=metadata.consume(ignored,sizeof(ignored));skipped+=part.consumed; }
    assert(skipped==result.tagBytes);
    const uint8_t mp3[]={0xff,0xfb,0x94};
    assert(metadata.consume(mp3,sizeof(mp3)).state==Mp3Metadata::State::Ready);
  }
  for (uint8_t version:{2,3,4}) {
    Mp3Metadata metadata;auto header=tag(version,0);
    assert(metadata.consume(header.data(),header.size()).consumed==10 && !metadata.pending());
  }
  Mp3Metadata footer;auto header=tag(4,20,0x10);
  auto result=footer.consume(header.data(),10);assert(result.tagBytes==40);
  uint8_t body[40]{};assert(footer.consume(body,sizeof(body)).consumed==30 && !footer.pending());
  // Multiple tags at the front are valid; an indexed range starts directly in audio.
  header=tag(3,0);Mp3Metadata chained;
  assert(chained.consume(header.data(),10).state==Mp3Metadata::State::Skipped);
  assert(chained.consume(header.data(),10).state==Mp3Metadata::State::Skipped);
  Mp3Metadata range(false);assert(range.consume(header.data(),10).state==Mp3Metadata::State::Ready);
  for (auto bad:{tag(1,0),tag(5,0),tag(3,0,1),tag(3,Mp3Metadata::MaxBytes),tag(4,0,1)}) {
    Mp3Metadata metadata;assert(metadata.consume(bad.data(),bad.size()).state==Mp3Metadata::State::Invalid);
  }
  header=tag(3,1);header[9]=0x80;Mp3Metadata invalid;
  assert(invalid.consume(header.data(),header.size()).state==Mp3Metadata::State::Invalid);
  puts("PASS: observed BR ID3 headers, bounded incremental skipping, ID3v2.2/3/4, footer, chained tags and invalid sizes/flags");
}
