#include "Radio.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
asm(".section .rodata\n.global _binary_x509_crt_bundle_start\n_binary_x509_crt_bundle_start:\n.byte 0\n.global _binary_x509_crt_bundle_end\n_binary_x509_crt_bundle_end:\n.previous");
using namespace leap;
static void worker() {
  try { radioTask(radioContext); } catch (const WorkerIdle &) {}
  RadioTrackSnapshot out;assert(!radio.trackInfo(out) && out.track.empty());
  assert(!radioAllocations && !fakeTlsClientsAlive);
}
int main() {
  assert(radio.begin());
  std::ifstream file("tests/fixtures/radio/podcast-vbr.mp3",std::ios::binary);
  std::vector<uint8_t> audio{std::istreambuf_iterator<char>(file),{}};
  assert(audio.size()>200000);auto part=audio;
  for (int i=0;i<3;++i) audio.insert(audio.end(),part.begin(),part.end());
  std::vector<uint8_t> wire;
  for (size_t i=0;i<audio.size();++i) {
    wire.push_back(audio[i]);
    if ((i+1)%4096) continue;
    std::string text;
    if (i+1==4096) text="StreamTitle='Ärzte - Erstes Lied';";
    if (i+1==65536) text="StreamTitle='Björk - Zweites Lied';";
    if (i+1==131072) text="StreamTitle='';";
    if (i+1==262144) text="StreamTitle='C - Drittes Lied';";
    size_t units=(text.size()+15)/16;wire.push_back(uint8_t(units));
    text.resize(units*16,0);wire.insert(wire.end(),text.begin(),text.end());
  }
  // Chunk boundaries cut through both metadata length/payload and MP3 frames.
  std::vector<uint8_t> chunks;
  for (size_t offset=0;offset<wire.size();) {
    size_t size=std::min(size_t(237),wire.size()-offset);
    char header[30];snprintf(header,sizeof(header),"%zx\r\n",size);
    chunks.insert(chunks.end(),header,header+strlen(header));
    chunks.insert(chunks.end(),wire.begin()+offset,wire.begin()+offset+size);
    chunks.push_back('\r');chunks.push_back('\n');offset+=size;
  }
  const char *tail="0\r\n\r\n";chunks.insert(chunks.end(),tail,tail+strlen(tail));
  auto enqueue=[&](bool icy) {
    std::map<std::string,String> headers={{"Content-Type","audio/mpeg"}};
    if (icy) { headers["icy-metaint"]="4096";headers["Transfer-Encoding"]="chunked"; }
    radioResponses.push_back({200,headers,{},icy ? chunks : audio});radio.play("https://radio.example/live");
  };
  uint64_t heard=0,hash=0,baseline=0,baselineHash=0;
  auto consume=[&] {
    int16_t out[2304];uint32_t rate=0;size_t frames=radio.read(out,1152,rate);heard+=frames;
    for (size_t i=0;i<frames*2;++i) hash=hash*1099511628211ULL+uint16_t(out[i]);
  };
  RadioTrackSnapshot display;
  radioOnDelay=[&] {
    radioMutexBusy=true;assert(!radio.trackInfo(display) && display.track.empty());
    radioMutexBusy=false;consume();
  };
  enqueue(false);worker();baseline=heard;baselineHash=hash;
  assert(baseline>44100*290);
  bool full=false,first=false,second=false,gapHeld=false,third=false,paused=false,resumed=false;
  uint32_t pauseAt=0;uint64_t pauseHeard=0;
  heard=hash=0;unsigned busyReads=0;
  radioOnDelay=[&] {
    bool has=radio.trackInfo(display);const auto &track=display.track;
    if (has) {
      RadioTrack before=track;radioMutexBusy=true;
      assert(radio.trackInfo(display) && display.track==before);
      radioMutexBusy=false;++busyReads;
    }
    if (!full) {
      assert(!has && !heard);full=radio.bufferedBytes()==512*1024;
      return;
    }
    if (has && std::string(track.title)=="Erstes Lied") {
      assert(!second && !third && std::string(track.artist)=="Ärzte" && heard>22050);first=true;
      if (!paused) { radio.pause(true);paused=true;pauseAt=radioNow;pauseHeard=heard; }
    }
    if (paused && !resumed) {
      assert(has && std::string(track.title)=="Erstes Lied" && heard==pauseHeard);
      radioNow+=100;
      if (radioNow-pauseAt<12000) return;
      radio.pause(false);resumed=true;
    }
    if (has && std::string(track.title)=="Zweites Lied") {
      assert(first && !third && heard>44100*8 && std::string(track.artist)=="Björk");second=true;
    }
    if (second && !third) {
      assert(has);
      if (heard>44100*40 && std::string(track.title)=="Zweites Lied") gapHeld=true;
    }
    if (has && std::string(track.title)=="Drittes Lied") {
      assert(gapHeld && heard>44100*30);third=true;
    }
    consume();
  };
  enqueue(true);worker();
  assert(full && first && second && gapHeld && third && resumed && busyReads>100);
  assert(heard==baseline && hash==baselineHash);
  assert(radioRequestHeaders.back().at("Icy-MetaData")=="1");
  // Stop and replacement fence published metadata immediately, even when the
  // replacement has no ICY support and old compressed audio remains queued.
  for (bool stopFirst : {true,false}) {
    bool switched=false;heard=hash=0;
    radioOnDelay=[&] {
      if (!switched && radio.trackInfo(display)) {
        switched=true;radioMutexBusy=true;
        if (stopFirst) {
          radio.stop();assert(!radio.trackInfo(display) && display.track.empty());
        }
        enqueue(false);assert(!radio.trackInfo(display) && display.track.empty());
        radioMutexBusy=false;return;
      }
      if (switched) assert(!radio.trackInfo(display));
      consume();
    };
    enqueue(true);worker();assert(switched);
  }
  // The optional ICY workspace is PSRAM-only and failure leaves the next play usable.
  radioFailAllocationBytes=sizeof(RadioIcyState);enqueue(true);worker();
  assert(radio.status()==RadioStatus::MemoryError);
  radioFailAllocationBytes=0;radioOnDelay=consume;enqueue(false);worker();
  puts("PASS: actual worker publishes ICY title/artist at PCM consumption, holds metadata in pause/read-ahead, retains on empty/mutex contention, clears on stop/switch, preserves PCM and releases optional workspace");
}
