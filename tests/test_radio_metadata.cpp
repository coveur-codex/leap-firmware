#include "RadioStream.h"
#include "DisplayText.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace leap;
static bool parse(IcyMetadata &parser,const std::string &wire,RadioTrack &out) {
  parser.begin();
  for (size_t i=0;i<wire.size();++i) {
    assert(!parser.ready());parser.feed(wire[i],i+1==wire.size());
  }
  return parser.take(out);
}
int main() {
  IcyMetadata parser;RadioTrack track;
  assert(parse(parser,"StreamTitle='Artist - My Song - Live';StreamUrl='https://x';",track));
  assert(std::string(track.artist)=="Artist" && std::string(track.title)=="My Song - Live");
  assert(!parser.take(track));
  assert(parse(parser,"StreamTitle='Guns N' Roses - Sweet Child O' Mine';",track));
  assert(std::string(track.artist)=="Guns N' Roses" && std::string(track.title)=="Sweet Child O' Mine");
  assert(parse(parser,"streamtitle=\"Björk – Jóga\";",track));
  assert(std::string(track.artist)=="Björk" && std::string(track.title)=="Jóga");
  assert(parse(parser,"StreamArtist='AC - DC';StreamTitle='Title';",track));
  assert(std::string(track.artist)=="AC - DC" && std::string(track.title)=="Title");
  assert(parse(parser,"StreamTitle='A - B';StreamArtist='Artist';",track));
  assert(std::string(track.artist)=="Artist" && std::string(track.title)=="A - B");
  assert(parse(parser,"StreamTitle='Nur ein Titel';",track) && !track.artist[0]);
  assert(std::string(track.title)=="Nur ein Titel");
  assert(parse(parser,"StreamTitle='';",track) && track.empty());
  strcpy(track.title,"Keep");
  for (auto wire : {"StreamUrl='https://x';", "StreamTitle='unterminated", "StreamTitle=no-quotes;", "garbage"})
    assert(!parse(parser,wire,track) && std::string(track.title)=="Keep");
  assert(parse(parser,"StreamTitle='Ärzte - Grüße';",track));
  assert(std::string(track.title)=="Grüße" && displayGlyphs(track.title).find(char(0x81))!=std::string::npos);
  assert(parse(parser,std::string("StreamTitle='Die \xc4rzte - Gr\xfc\xdf" "e';"),track));
  assert(std::string(track.artist)=="Die Ärzte" && std::string(track.title)=="Grüße");
  assert(parse(parser,"StreamTitle=' A\n\t -  B\x01\r\n ';",track));
  assert(std::string(track.artist)=="A" && std::string(track.title)=="B");
  assert(parse(parser,R"(StreamTitle='A - It\'s \"quoted\"';)",track));
  assert(std::string(track.title)=="It's \"quoted\"");
  std::string longTitle;
  for (unsigned i=0;i<1000;++i) longTitle+="ö";
  assert(parse(parser,"StreamTitle='Artist - "+longTitle+"';",track));
  assert(strlen(track.title)==192 && std::string(track.title).substr(190)=="ö");
  // Full legal ICY block, padded to exactly 4080 bytes; unknown fields ignored.
  std::string full="StreamTitle='A - B';";full.resize(4080,0);
  assert(parse(parser,full,track) && std::string(track.title)=="B");
  // Arbitrary HTTP chunk and ICY splits, zero-length unchanged.
  IcyMetadata meta;RadioBody body(true,3,&meta);std::string audio;
  std::string block="StreamTitle='A - B';";block.resize(32,0);
  std::string wire=std::string("abc")+char(2)+block+"def"+char(0)+"ghi";
  unsigned updates=0;
  for (char byte : wire) {
    std::string chunk="1\r\n";chunk+=byte;chunk+="\r\n";
    for (uint8_t c : chunk) {
      int kind=body.feed(c);assert(kind>=0);if (kind==1) audio+=char(c);
      if (meta.take(track)) { ++updates;assert(std::string(track.title)=="B"); }
    }
  }
  assert(audio=="abcdefghi" && updates==1);
  // Metadata cannot jump ahead of compressed or PCM read-ahead, even in pause.
  RadioTrackTimeline timeline;
  timeline.push(100,track);timeline.map(99,0);assert(!timeline.take(1000,track));
  timeline.map(100,2000);assert(!timeline.take(2000,track));assert(timeline.take(2001,track));
  RadioTrack empty;
  timeline.push(150,empty);timeline.map(150,2500);assert(!timeline.take(2501,track));
  timeline.push(200,track);timeline.map(200,3000);assert(!timeline.take(4000,track)); // duplicate
  RadioTrack other;strcpy(other.title,"Next");timeline.push(300,other);
  timeline.map(300,UINT32_MAX-10);assert(!timeline.take(UINT32_MAX-10,track));
  assert(timeline.take(5,track) && std::string(track.title)=="Next");
  // An overfull future history coalesces the most distant event without growing.
  RadioTrackTimeline crowded;
  for (unsigned i=0;i<100;++i) { snprintf(other.title,sizeof(other.title),"Song %u",i);crowded.push(i,other); }
  crowded.map(62,100);assert(crowded.take(101,track) && std::string(track.title)=="Song 62");
  crowded.map(98,200);assert(!crowded.take(201,track));
  crowded.map(99,200);assert(crowded.take(201,track) && std::string(track.title)=="Song 99");
  RadioTrackTimeline mapped;
  for (unsigned i=0;i<64;++i) { snprintf(other.title,sizeof(other.title),"Song %u",i);mapped.push(i,other); }
  mapped.map(63,0);strcpy(other.title,"Retry");mapped.push(64,other);
  assert(mapped.take(1,track) && std::string(track.title)=="Song 63");
  mapped.push(64,other);mapped.map(64,10);
  assert(mapped.take(11,track) && std::string(track.title)=="Retry");
  puts("PASS: bounded ICY title/artist parsing, quotes, UTF-8/Latin-1, empty/malformed/padded blocks, chunk boundaries and playback-timed metadata");
}
