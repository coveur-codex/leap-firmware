#include "Radio.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
asm(".section .rodata\n.global _binary_x509_crt_bundle_start\n_binary_x509_crt_bundle_start:\n.byte 0\n.global _binary_x509_crt_bundle_end\n_binary_x509_crt_bundle_end:\n.previous");
using namespace leap;
static uint64_t heard=0;
void run() {
  try { radioTask(radioContext); } catch (const WorkerIdle &) {}
  assert(radioAllocations==0);
}
int main() {
  assert(radio.begin());
  std::ifstream file("tests/fixtures/radio/podcast-vbr.mp3",std::ios::binary);
  std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
  assert(bytes.size()>50000);
  const char *url="https://podcast.example/episode.mp3";
  unsigned stage=0;
  bool paused=false,resumed=false;
  radioOnDelay=[&] {
    if (stage==5 && !paused && radio.position()>=5) {
      radio.pause(true); paused=true;
      int16_t out[256]; uint32_t rate=0;
      assert(radio.status()==RadioStatus::Paused && !radio.read(out,128,rate)); return;
    }
    if (stage==5 && paused && !resumed) { radio.pause(false); resumed=true; }
    int16_t out[2304]; uint32_t rate=0;
    heard+=radio.read(out,1152,rate);
  };
  auto enqueue=[&] {
    if (stage==0 || stage==1 || stage==3)
      radioResponses.push_back({200,{{"Content-Type","audio/mpeg"},{"ETag","v1"}},{},bytes,true});
    else if (stage==4)
      radioResponses.push_back({200,{{"Content-Type","audio/mpeg"},{"Content-Length",String(bytes.size()+100)}},{},bytes,false});
    else radioResponses.push_back({200,{{"Content-Type","audio/mpeg"}},{},bytes,false});
    uint32_t pos=stage==1 ? 45 : stage==2 ? 60 : stage==3 ? 75 : 0;
    radio.playEpisode(url,pos,75,pos!=0);
  };
  radioOnIdle=[&] {
    if (stage==0) assert(radio.status()==RadioStatus::Ended && radio.naturalEnd() && heard>44100*74);
    if (stage==1) {
      assert(radio.status()==RadioStatus::Ended && radio.naturalEnd());
      assert(heard>=44100*30 && heard<44100*31);
      assert(radioRequestHeaders.back().count("Range") && radioRequestHeaders.back().at("If-Range")=="v1");
    }
    if (stage==2) {
      assert(radioRequestHeaders.back().count("Range"));
      assert(radio.status()==RadioStatus::Ended && heard>=44100*15 && heard<44100*16);
    }
    if (stage==3) assert(radio.status()==RadioStatus::Ended && !radio.naturalEnd());
    if (stage==4) assert(radio.status()!=RadioStatus::Ended && !radio.naturalEnd());
    if (stage==5) { assert(paused && resumed && radio.naturalEnd()); radioOnIdle=nullptr; return; }
    heard=0; ++stage; enqueue();
  };
  enqueue(); run(); assert(stage==5);
  puts("PASS: actual shared MP3 worker: VBR sample seeks, indexed HTTP Range, ignored Range, EOF/tail drain, forced end, truncation and pause/resume");
}
