#include "Radio.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
asm(".section .rodata\n.global _binary_x509_crt_bundle_start\n_binary_x509_crt_bundle_start:\n.byte 0\n.global _binary_x509_crt_bundle_end\n_binary_x509_crt_bundle_end:\n.previous");
using namespace leap;
static uint64_t heard=0, reference=0;
static unsigned stage=0;
static const uint32_t tagSizes[]={207533,248904,163691};
std::vector<uint8_t> tagged(const std::vector<uint8_t> &audio,uint32_t size) {
  std::vector<uint8_t> body(size,0);
  body[0]='I';body[1]='D';body[2]='3';body[3]=3;
  uint32_t payload=size-10;
  for (int i=9;i>=6;--i) { body[i]=payload&127;payload>>=7; }
  // Artwork is opaque metadata, even if it contains decodable MP3 frames.
  std::copy_n(audio.begin(),std::min(audio.size(),size_t(8192)),body.begin()+128);
  body.insert(body.end(),audio.begin(),audio.end());
  return body;
}
int main(int argc,char **argv) {
  assert(radio.begin());
  std::ifstream file("tests/fixtures/radio/podcast-vbr.mp3",std::ios::binary);
  std::vector<uint8_t> audio{std::istreambuf_iterator<char>(file),{}};
  assert(!audio.empty());
  auto enqueue=[&] {
    std::vector<uint8_t> body;
    if (!stage) body=audio;
    else if (stage<=3) body=tagged(audio,tagSizes[stage-1]);
    else if (stage<=6) body=tagged(audio,tagSizes[2]);
    else if (stage==7) body={'I','D','3',3,0,0,0,0,0,0x80};
    else if (stage==8) { body=tagged(audio,tagSizes[0]);body.resize(1000); }
    else if (stage==9) body.assign(150000,0);
    else body=tagged(audio,tagSizes[0]);
    if (stage==1) radioResponses.push_back({301,{{"Location","https://cdn.example/episode.mp3"}},{}});
    radioResponses.push_back({200,{{"Content-Type","audio/mpeg"},{"Content-Length",stage==8 ? String("") : String(body.size())},{"ETag","v1"}},{},body,stage!=6});
    uint32_t position=stage==4 ? 30 : stage==5 || stage==6 ? 1 : 0;
    String url=stage>=3 && stage<=6 ? "https://media.example/last.mp3" : String("https://media.example/")+String(stage)+".mp3";
    radio.playEpisode(url,position,75,position!=0);
  };
  radioOnDelay=[] {
    if (stage==10) { radio.stop();return; }
    int16_t out[2304];uint32_t rate=0;heard+=radio.read(out,1152,rate);
    if (stage==1) radioNow+=100; // Valid metadata transfer may take longer than 8 seconds.
  };
  radioOnIdle=[&] {
    if (!stage) { reference=heard;assert(reference>44100*74); }
    if (stage<=6) {
      assert(radio.status()==RadioStatus::Ended && radio.naturalEnd());
      uint32_t seek=stage==4 ? 30 : stage==5 || stage==6 ? 1 : 0;
      assert(heard==reference-uint64_t(seek)*44100);
      if (stage>=4) {
        auto range=radioRequestHeaders.back().at("Range");
        assert(strtoul(range.c_str()+6,nullptr,10)>=tagSizes[2]);
      }
    } else if (stage==7 || stage==8) assert(radio.status()==RadioStatus::InvalidMetadata);
    else if (stage==9) assert(radio.status()==RadioStatus::Unsupported);
    else { assert(radio.status()==RadioStatus::Stopped && !heard);radioOnIdle=nullptr;return; }
    heard=0;++stage;enqueue();
  };
  enqueue();try { radioTask(radioContext); } catch (const WorkerIdle &) {}
  assert(stage==10 && radioAllocations==0);
  // Optional bounded prefixes from a live provider; no audio is kept as a fixture.
  for (int i=1;i<argc;++i) {
    std::ifstream prefix(argv[i],std::ios::binary);assert(prefix.good());
    std::vector<uint8_t> body{std::istreambuf_iterator<char>(prefix),{}};
    assert(!body.empty() && body.size()<=2*1024*1024);
    radioResponses.push_back({200,{{"Content-Type","audio/mpeg"},{"Content-Length",String(body.size()+1)}},{},body,false});
    heard=0;
    radioOnDelay=[] { int16_t out[2304];uint32_t rate=0;heard+=radio.read(out,1152,rate); };
    radio.playEpisode("https://provider.example/prefix.mp3");
    try { radioTask(radioContext); } catch (const WorkerIdle &) {}
    assert(heard>0 && radio.status()!=RadioStatus::Unsupported && radio.status()!=RadioStatus::InvalidMetadata && radioAllocations==0);
    printf("PASS: live provider prefix %s produces decoded PCM\n",argv[i]);
  }
  puts("PASS: real worker skips large BR-sized ID3/artwork, preserves PCM/seek offsets, handles slow tags, truncation and cancellation, bounds random junk");
}
