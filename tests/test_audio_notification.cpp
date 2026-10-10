#include "Audio.h"
#include <driver/i2s_std.h>
#include <cassert>
#include <fstream>
#include <vector>
using namespace leap;
struct Complete {};
int main() {
  std::vector<uint8_t> wav(44+44100,0);
  auto word=[&](size_t at,uint32_t value,int bytes){ for(int i=0;i<bytes;++i) wav[at+i]=(value>>(i*8))&255; };
  memcpy(wav.data(),"RIFF",4);word(4,wav.size()-8,4);
  memcpy(wav.data()+8,"WAVEfmt ",8);word(16,16,4);word(20,1,2);word(22,1,2);
  word(24,22050,4);word(28,44100,4);word(32,2,2);word(34,16,2);
  memcpy(wav.data()+36,"data",4);word(40,44100,4);
  for(size_t at=44;at<wav.size();at+=2)word(at,1000,2);
  const char *path="build/tests/notification-music.wav";
  std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char *>(wav.data()),wav.size());file.close();
  assert(Audio::validateWav(path));
  for(int volume:{35,0}) {
    assert(audio.begin()); audio.volume=volume;
    assert(audio.play(path));
    unsigned blocks=0;bool mixed=false,resumed=false;
    fakeAudioWrite=[&](const int16_t *samples,size_t size){
      ++blocks;
      if(blocks==1) {
        for(size_t i=0;i<size;++i)assert(samples[i]==volume*10);
        audio.notify(); // Arrives during a long WAV, with no queued tone job.
      } else if(blocks==2) {
        for(size_t i=0;i<size;++i)if(samples[i]!=volume*10)mixed=true;
      } else if(blocks==40) {
        for(size_t i=0;i<size;++i)assert(samples[i]==volume*10);
        resumed=true; // WAV continues after the chime; no stop/restart.
      }
      if(volume==0)for(size_t i=0;i<size;++i)assert(samples[i]==0);
      if(blocks==45)throw Complete{};
    };
    try { fakeTask(fakeContext); } catch(const Complete &) {}
    assert(blocks==45 && resumed && (volume==0 || mixed));
  }
  puts("PASS: actual Audio task mixes notifications into a running WAV immediately, keeps music playing and honors mute");
}
