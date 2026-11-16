#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
namespace leap {
struct RadioTrack {
  char artist[129]{};
  char title[193]{};
  bool empty() const { return !artist[0] && !title[0]; }
  bool operator==(const RadioTrack &other) const {
    return !strcmp(artist,other.artist) && !strcmp(title,other.title);
  }
};
// The UI keeps this snapshot between frames. An unavailable mutex must not
// erase known text; the epoch prevents it from surviving a session change.
struct RadioTrackSnapshot {
  RadioTrack track;
  uint32_t epoch=0;
};
namespace icy_detail {
inline bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
inline std::string_view trim(std::string_view s) {
  while (!s.empty() && space(s.front())) s.remove_prefix(1);
  while (!s.empty() && space(s.back())) s.remove_suffix(1);
  return s;
}
inline bool key(std::string_view s,const char *expected) {
  s=trim(s);
  if (s.size()!=strlen(expected)) return false;
  for (size_t i=0;i<s.size();++i) {
    char c=s[i];if (c>='A' && c<='Z') c+=32;
    if (c!=expected[i]) return false;
  }
  return true;
}
// Keep valid UTF-8, convert legacy Latin-1 bytes, strip controls and never
// truncate a codepoint. No allocation based on untrusted stream text.
template<size_t N> void clean(char (&out)[N],std::string_view text) {
  size_t written=0;
  for (size_t i=0;i<text.size();) {
    uint8_t first=uint8_t(text[i]);uint32_t code=first;size_t bytes=1;
    if (first=='\\' && i+1<text.size() && (text[i+1]=='\'' || text[i+1]=='"' || text[i+1]=='\\')) {
      code=uint8_t(text[i+1]);bytes=2;
    } else if (first>=0xc2 && first<=0xf4) {
      size_t count=first<0xe0 ? 2 : first<0xf0 ? 3 : 4;
      uint32_t value=first & (count==2 ? 31 : count==3 ? 15 : 7);
      bool valid=i+count<=text.size();
      for (size_t j=1;valid && j<count;++j) {
        uint8_t c=uint8_t(text[i+j]);valid=(c&0xc0)==0x80;value=(value<<6)|(c&63);
      }
      if (valid && value>=(count==2 ? 0x80u : count==3 ? 0x800u : 0x10000u) &&
          value<=0x10ffff && !(value>=0xd800 && value<=0xdfff)) { code=value;bytes=count; }
    }
    i+=bytes;
    if (code=='\t' || code=='\r' || code=='\n' || code==0xa0) code=' ';
    if (code<32 || (code>=127 && code<160)) continue;
    if (code==' ' && (!written || out[written-1]==' ')) continue;
    char encoded[4];size_t count=0;
    if (code<128) encoded[count++]=char(code);
    else if (code<0x800) { encoded[count++]=char(0xc0|(code>>6));encoded[count++]=char(0x80|(code&63)); }
    else if (code<0x10000) {
      encoded[count++]=char(0xe0|(code>>12));encoded[count++]=char(0x80|((code>>6)&63));encoded[count++]=char(0x80|(code&63));
    } else {
      encoded[count++]=char(0xf0|(code>>18));encoded[count++]=char(0x80|((code>>12)&63));
      encoded[count++]=char(0x80|((code>>6)&63));encoded[count++]=char(0x80|(code&63));
    }
    if (written+count>=N) break;
    memcpy(out+written,encoded,count);written+=count;
  }
  while (written && out[written-1]==' ') --written;
  out[written]=0;
}
}
class IcyMetadata {
  char block[4081]; // ICY's one-byte length permits at most 255 * 16 bytes.
  size_t used=0;
  bool complete=false;
public:
  bool ready() const { return complete; }
  void begin() { used=0;complete=false; }
  void feed(uint8_t byte,bool last) {
    if (used<sizeof(block)-1) block[used++]=char(byte);
    if (last) { block[used]=0;complete=true; }
  }
  // A zero-length block means unchanged; explicit empty fields parse as empty.
  bool take(RadioTrack &track) {
    if (!complete) return false;
    complete=false;
    std::string_view data(block,strlen(block)), title,artist;
    bool hasTitle=false,hasArtist=false;
    while (!data.empty()) {
      data=icy_detail::trim(data);
      if (data.empty()) break;
      auto equal=data.find('=');
      if (equal==data.npos) break;
      auto name=data.substr(0,equal);data.remove_prefix(equal+1);data=icy_detail::trim(data);
      if (data.empty() || (data.front()!='\'' && data.front()!='"')) break;
      char quote=data.front();data.remove_prefix(1);size_t end=0;
      for (;end<data.size();++end) {
        if (data[end]!=quote) continue;
        // Apostrophes inside names are common, including unescaped ones.
        size_t slashes=0;
        for (size_t i=end;i && data[i-1]=='\\';--i) ++slashes;
        if (slashes%2) continue;
        auto tail=icy_detail::trim(data.substr(end+1));
        if (tail.empty() || tail.front()==';') break;
      }
      if (end==data.size()) break; // Incomplete/malformed field is ignored.
      if (icy_detail::key(name,"streamtitle")) { title=data.substr(0,end);hasTitle=true; }
      if (icy_detail::key(name,"streamartist")) { artist=data.substr(0,end);hasArtist=true; }
      data=icy_detail::trim(data.substr(end+1));
      if (!data.empty()) data.remove_prefix(1);
    }
    if (!hasTitle) return false;
    track={};title=icy_detail::trim(title);
    if (!hasArtist) {
      size_t split=title.npos,width=0;
      for (auto delimiter : {std::string_view(" - "),std::string_view(" – "),std::string_view(" — ")}) {
        auto at=title.find(delimiter);
        if (at!=title.npos && (split==title.npos || at<split)) { split=at;width=delimiter.size(); }
      }
      if (split!=title.npos && split && split+width<title.size()) {
        artist=title.substr(0,split);title.remove_prefix(split+width);
      }
    }
    icy_detail::clean(track.artist,artist);icy_detail::clean(track.title,title);
    return true;
  }
};
// Bounded metadata history shares the existing worker with the audio pipeline.
// Byte markers become PCM markers only as the decoder reaches them; publication
// follows actual PCM consumption, never the HTTP read-ahead position.
class RadioTrackTimeline {
  struct Event { uint64_t byte;uint32_t pcm;bool mapped;RadioTrack track; };
  static constexpr size_t Capacity=64;
  Event events[Capacity];
  size_t head=0,count=0;
  RadioTrack last{};
  bool hasLast=false;
public:
  void push(uint64_t byte,const RadioTrack &track) {
    if (track.empty() || (hasLast && last==track)) return;
    if (count==Capacity) {
      // Preserve imminent changes, coalesce only the most distant future event.
      // Never overwrite a marker that is already waiting for PCM consumption.
      auto &tail=events[(head+count-1)%Capacity];
      if (!tail.mapped) { tail={byte,0,false,track};last=track;hasLast=true; }
      return;
    }
    last=track;hasLast=true;
    events[(head+count)%Capacity]={byte,0,false,track};++count;
  }
  void map(uint64_t byte,uint32_t pcm) {
    for (size_t i=0;i<count;++i) {
      auto &event=events[(head+i)%Capacity];
      if (event.byte>byte) break;
      if (!event.mapped) { event.pcm=pcm;event.mapped=true; }
    }
  }
  bool take(uint32_t consumed,RadioTrack &track) {
    bool changed=false;
    while (count) {
      auto &event=events[head];
      if (!event.mapped || int32_t(consumed-event.pcm)<=0) break;
      track=event.track;head=(head+1)%Capacity;--count;changed=true;
    }
    return changed;
  }
};
struct RadioIcyState { IcyMetadata parser;RadioTrackTimeline timeline;RadioTrack scratch; };
} // namespace leap
