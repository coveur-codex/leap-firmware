#include "Radio.h"
#include "Mp3ReadAhead.h"
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
  assert(!radio.active() && radio.bufferedBytes()==0 && radioAllocations==0 && fakeTlsClientsAlive==0);
  int16_t out[256]; uint32_t rate=0; assert(!radio.read(out,128,rate));
}
static void pureQueue() {
  // Wrap both write and read pointers repeatedly, with unequal producer/consumer blocks.
  uint8_t memory[17], out[13]; Mp3ReadAhead queue(memory,sizeof(memory));
  unsigned produced=0,consumed=0;
  for (unsigned round=0;round<10000;++round) {
    for (unsigned i=0;i<11 && queue.free();++i) assert(queue.push(uint8_t(produced++)));
    auto count=queue.read(out,std::min(size_t(7),queue.size()));
    for (size_t i=0;i<count;++i) assert(out[i]==uint8_t(consumed++));
    assert(queue.size()==produced-consumed);
  }
  while (queue.free()) assert(queue.push(uint8_t(produced++)));
  assert(!queue.push(42)); queue.clear(); assert(queue.size()==0 && queue.free()==17);
  assert(!queue.read(out,sizeof(out)));
  // Unsigned clocks survive millis() overflow; pause and buffered playback reset budgets.
  Mp3StreamWatch watch(UINT32_MAX-4000);
  assert(!watch.networkExpired(3000,false,false,false));
  assert(watch.networkExpired(4000,false,false,false));
  assert(!watch.networkExpired(20000,true,false,false));
  assert(!watch.consumerExpired(20000,0,1024,true,true));
  assert(!watch.consumerExpired(29000,0,1024,true,false));
  assert(watch.consumerExpired(30000,0,1024,true,false));
}
int main() {
  pureQueue(); assert(radio.begin());
  std::ifstream file("tests/fixtures/radio/podcast-vbr.mp3",std::ios::binary);
  std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
  auto part=bytes;
  for (unsigned i=0;i<3;++i) bytes.insert(bytes.end(),part.begin(),part.end());
  assert(bytes.size()>Mp3ReadAhead::Bytes*2);
  const char *url="https://podcast.example/buffer.mp3";
  auto enqueue=[&](const std::vector<uint8_t> &body,bool finite=true) {
    radioResponses.push_back({200,{{"Content-Type","audio/mpeg"}},{},body});
    if (finite) radio.playEpisode(url,0,75); else radio.play(url);
  };
  uint64_t heard=0,baseline=0,checksum=0,baselineChecksum=0;
  auto consume=[&] {
    int16_t out[2304];uint32_t rate=0;
    size_t frames=radio.read(out,1152,rate);heard+=frames;
    for (size_t i=0;i<frames*2;++i) checksum=checksum*1099511628211ULL+uint16_t(out[i]);
  };
  radioOnDelay=consume; enqueue(bytes); worker();
  assert(radio.status()==RadioStatus::Ended); baseline=heard;baselineChecksum=checksum;
  // Full reserve while paused, hold >8s, then a 9s source outage while buffered
  // audio continues. Compare the PCM count and checksum with uninterrupted playback.
  heard=0;checksum=0; bool filled=false,paused=false,recovered=false; uint32_t heldAt=0,stallAt=0;
  radioOnDelay=[&] {
    if (!paused && radio.status()==RadioStatus::Playing) { radio.pause(true);paused=true; }
    if (paused && !filled && radio.bufferedBytes()==Mp3ReadAhead::Bytes) {
      filled=true;heldAt=radioNow;
    }
    if (!filled) return;
    if (!stallAt) {
      radioNow+=100;
      assert(radio.active());
      if (radioNow-heldAt>=12000) { radio.pause(false);radioNetworkStalled=true;stallAt=radioNow; }
      return;
    }
    radioNow+=23; consume(); // Roughly 44.1 kHz real-time playback.
    if (radioNow-stallAt>=9000) { radioNetworkStalled=false;recovered=true; }
  };
  enqueue(bytes); worker();
  assert(filled && recovered && radio.status()==RadioStatus::Ended && heard==baseline && checksum==baselineChecksum);
  // Exhaust the reserve during a sustained outage, then report a timeout rather
  // than waiting forever or misclassifying the source as a non-MP3 stream.
  heard=0;checksum=0;bool outage=false;uint32_t outageAt=0;
  radioOnDelay=[&] {
    if (!outage) {
      if (radio.bufferedBytes()!=Mp3ReadAhead::Bytes) return;
      outage=true;outageAt=radioNow;radioNetworkStalled=true;
    }
    radioNow+=23;consume();
  };
  enqueue(bytes);worker();
  assert(outage && heard>0 && heard<baseline && radio.status()==RadioStatus::Timeout);
  assert(radioNow-outageAt>9000 && radioNow-outageAt<200000);
  radioNetworkStalled=false;
  // Chunk and ICY boundaries cross the circular buffer and decoder window.
  // The wire metadata must never affect the decoded sample count.
  auto chunked=[](const std::vector<uint8_t> &input) {
    std::vector<uint8_t> wire;
    for (size_t offset=0;offset<input.size();) {
      size_t size=std::min(size_t(1537),input.size()-offset);
      char header[40];snprintf(header,sizeof(header),"%zx;test=1\r\n",size);
      wire.insert(wire.end(),header,header+strlen(header));
      wire.insert(wire.end(),input.begin()+offset,input.begin()+offset+size);
      wire.push_back('\r');wire.push_back('\n');offset+=size;
    }
    const char *tail="0\r\n\r\n";wire.insert(wire.end(),tail,tail+strlen(tail));return wire;
  };
  for (bool icy : {false,true}) {
    std::vector<uint8_t> framed;
    for (size_t i=0;i<bytes.size();++i) {
      framed.push_back(bytes[i]);
      if (icy && (i+1)%197==0) { framed.push_back(2);framed.insert(framed.end(),32,0xff); }
    }
    auto wire=chunked(framed);heard=0;checksum=0;bool full=false;
    radioOnDelay=[&] {
      if (!full) full=radio.bufferedBytes()==Mp3ReadAhead::Bytes;
      if (full) consume();
    };
    std::map<std::string,String> headers={{"Content-Type","audio/mpeg"},{"Transfer-Encoding","chunked"}};
    if (icy) headers["icy-metaint"]="197";
    radioResponses.push_back({200,headers,{},wire});
    if (icy) radio.play(url); else radio.playEpisode(url);
    worker();
    assert(full && heard==baseline && checksum==baselineChecksum);
    assert(radio.status()==(icy ? RadioStatus::ConnectionError : RadioStatus::Ended));
  }
  // A dead audio task is bounded, both during streaming and when draining EOF.
  radioOnDelay=nullptr; uint32_t started=radioNow;
  enqueue(bytes); worker();
  assert(radio.status()==RadioStatus::AudioStalled && radioNow-started<20000);
  std::vector<uint8_t> shortBody(bytes.begin(),bytes.begin()+1000);
  started=radioNow;radioSerial.clear(); enqueue(shortBody); worker();
  assert(radio.status()==RadioStatus::AudioStalled && radioNow-started<20000);
  assert(radioSerial.find(" playing rate=")==std::string::npos);
  assert(radioSerial.find("pcm_queued=")!=std::string::npos);
  // Empty connected socket: no frames/PCM are available to mask the idle timeout.
  radioKeepConnected=true;radioNetworkStalled=true;started=radioNow;
  enqueue(bytes); worker();
  assert(radio.status()==RadioStatus::Timeout && radioNow-started<15000);
  radioKeepConnected=false;radioNetworkStalled=false;
  // Allocation failure is explicit, leaks nothing, and the next play recovers.
  radioFailAllocationBytes=Mp3ReadAhead::Bytes;radio.play(url);worker();
  assert(radio.status()==RadioStatus::MemoryError);
  radioFailAllocationBytes=0;heard=0;checksum=0;radioOnDelay=consume;enqueue(bytes);worker();
  assert(radio.status()==RadioStatus::Ended && heard==baseline && checksum==baselineChecksum);
  // Switch with a full buffer: generation fences immediately reject old PCM.
  bool switched=false;heard=0;checksum=0;
  radioOnDelay=[&] {
    if (!switched) {
      if (radio.bufferedBytes()<Mp3ReadAhead::Bytes) return;
      switched=true;radio.stop();
      int16_t out[256];uint32_t rate=0;
      assert(!radio.read(out,128,rate) && !radio.bufferedBytes());
      enqueue(shortBody); // Same sequence as radio page entry / podcast -> radio.
      assert(!radio.read(out,128,rate) && !radio.bufferedBytes());
    } else consume();
  };
  enqueue(bytes,false);worker();assert(switched && radio.status()==RadioStatus::Ended);
  assert(heard>0 && heard<baseline);
  assert(radioReadAheadAllocations>=8);
  puts("PASS: 512-KiB PSRAM reserve, wraparound, long pause/network outage, stalled output/EOF, timeout, allocation recovery and page-switch cleanup");
}
