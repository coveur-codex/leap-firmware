#include "AudioControls.h"
#include "PodcastPlayback.h"
#include "Core.h"
#include <cassert>
#include <cstdio>
using namespace leap;
int main() {
  assert(AudioControls::volumeDelta(false,0)==5);
  assert(AudioControls::volumeDelta(false,1)==-5);
  for (int key=0;key<5;++key) assert(!AudioControls::volumeDelta(true,key));
  assert(AudioControls::stationDelta(true,0)==1 && AudioControls::stationDelta(true,1)==-1);
  for (int key=2;key<5;++key) assert(!AudioControls::stationDelta(true,key));
  assert(AudioControls::adjustVolume(59,5)==60 && AudioControls::adjustVolume(1,-5)==0);
  for (int volume=0;volume<10;++volume) assert(AudioControls::adjustVolume(volume,5)==volume+1);
  for (int volume=1;volume<=10;++volume) assert(AudioControls::adjustVolume(volume,-5)==volume-1);
  assert(AudioControls::adjustVolume(0,-5)==0 && AudioControls::adjustVolume(10,5)==15);
  assert(AudioControls::adjustVolume(15,-5)==10 && AudioControls::adjustVolume(12,-5)==10);
  assert(AudioControls::adjustVolume(60,5)==60 && AudioControls::adjustVolume(8,0)==8);
  assert(podcastSeek(10,-15,100)==0 && podcastSeek(90,30,100)==100);
  assert(podcastSeek(90,30,0)==120 && podcastSeek(UINT32_MAX,30,0)==7*86400);
  assert(!podcastNaturalEnd(true,4) && podcastNaturalEnd(true,5) && podcastNaturalEnd(false,0));
  Mp3SeekIndex index;
  index.add(100,0,44100);index.add(17000,44100*30,44100);index.add(52000,44100*60,44100);
  assert(index.before(31).offset==100 && index.before(45).offset==17000);
  assert(index.before(62).offset==52000);
  // Short back is emitted only at debounced release, long back never emits short.
  Debouncer key;
  assert(!key.poll(true,0,false,0,true));
  assert(!key.poll(true,25,false,0,true));
  assert(!key.poll(false,100,false,0,true));
  assert(key.poll(false,125,false,0,true)==1);
  assert(!key.poll(true,200,false,0,true));assert(!key.poll(true,225,false,0,true));
  assert(key.poll(true,1125,false,0,true)==3);
  assert(!key.poll(true,1400,true,0,true));
  assert(!key.poll(false,1450,false,0,true));assert(!key.poll(false,1475,false,0,true));
  puts("PASS: common audio keys, safe volume limits, seek clamps, VBR sample checkpoints and exclusive short/long back");
}
