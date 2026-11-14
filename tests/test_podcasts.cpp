#include "Podcasts.h"
#include "Transport.h"
#include "Audio.h"
#include "Radio.h"
#include <cassert>
#include <functional>
#include <map>
#include <memory>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) { return malloc(n); }
void RamAllocator::deallocate(void *p) { free(p); }
void *RamAllocator::reallocate(void *p,size_t n) { return realloc(p,n); }
Storage storage;
Audio audio;
Radio radio;
static JsonDocument config, saved;
static std::map<std::string,std::string> files;
static std::function<bool(const String &,JsonDocument &,JsonDocument *)> http;
static String playedUrl;
static unsigned playCount=0;
static bool eof=false, natural=true;
size_t Storage::freeBytes() const { return 1024*1024; }
bool Storage::load(JsonDocument &out,TickType_t,bool *,JsonDocument *,std::shared_ptr<const JsonDocument> *) { out["config"]=config;return true; }
bool Storage::readJson(const String &path,JsonDocument &out,size_t) {
  if (!files.count(path)) { out.clear(); return false; }
  return !deserializeJson(out,files[path]);
}
bool Storage::writeJson(const String &path,JsonDocument &doc,size_t) { std::string value;serializeJson(doc,value);files[path]=value;return true; }
bool Transport::json(const String &path,JsonDocument &out,JsonDocument *body,uint32_t) { status=200; bool ok=http(path,out,body);if(!ok)status=503;return ok; }
void Audio::tone(uint16_t,uint16_t) {}
void Radio::queue(const String &url,uint32_t position,uint32_t duration,bool,bool) {
  playedUrl=url;++playCount;requestedEpoch=++generation;positionSeconds=position;durationSeconds=duration;
  pausedFlag=false;endedNaturally=false;state=RadioStatus::Playing;
}
void Radio::stop() { requestedEpoch=0;++generation;pausedFlag=false;state=RadioStatus::Stopped; }
size_t Radio::read(int16_t *,size_t seconds,uint32_t &) {
  if (eof) { endedNaturally=natural;state=RadioStatus::Ended;requestedEpoch=0; }
  else if (!pausedFlag) positionSeconds=positionSeconds+seconds;
  return 0;
}
}
static JsonDocument episodes;
static void putEpisode(JsonVariant target,int index,const char *state="unheard",int position=0) {
  char id[65];snprintf(id,sizeof(id),"%064x",index);
  target["id"]=id;target["podcastId"]=1;target["title"]=String("Episode ")+String(index);
  target["url"]=String("https://example.org/")+String(index)+".mp3";
  target["position"]=position;target["duration"]=120;target["status"]=state;
  target["revision"]=0;target["publishedAt"]=String(100-index);
}
int main() {
  config["pages"][0]["id"]="podcasts";config["pages"][0]["enabled"]=true;
  config["podcasts"][0]["id"]=1;config["podcasts"][0]["name"]="Wissen";
  config["podcasts"][0]["episodeCount"]=30;
  assert(podcastInbox.begin());
  Transport net;
  unsigned requests=0,uploads=0;
  http=[&](const String &path,JsonDocument &out,JsonDocument *body) {
    ++requests;
    if (body) { assert(path.find("/progress")!=std::string::npos);saved.set(*body);++uploads;out["ok"]=true;return true; }
    if (path.find("/resume")!=std::string::npos) { out["episode"]=nullptr;return true; }
    if (path.find("/next")!=std::string::npos) { putEpisode(out["episode"].to<JsonObject>(),15);out["offset"]=15;return true; }
    if (path.find("?offset=")!=std::string::npos) {
      int offset=std::stoi(path.substr(path.find("?offset=")+8));
      out["podcastId"]=1;out["offset"]=offset;out["total"]=30;
      auto rows=out["episodes"].to<JsonArray>();
      for (int i=offset;i<std::min(30,offset+12);++i) putEpisode(rows.add<JsonObject>(),i,i==13 ? "partial" : "unheard",i==13 ? 15 : 0);
      return true;
    }
    int index=std::stoi(path.substr(path.size()-3),nullptr,16);
    putEpisode(out["episode"].to<JsonObject>(),index,index==13 ? "partial" : "unheard",index==13 ? 15 : 0);out["offset"]=index;return true;
  };
  bool online=true;
  auto service=[&] { podcastInbox.service(net,"/api/v1/devices/child",online); };
  Podcasts podcasts;
  auto tick=[&] { podcasts.show(true,config["podcasts"].as<JsonArrayConst>()); };
  auto key=[&](Key key,bool longPress=false) { podcasts.action({true,key,longPress}); };
  tick();assert(podcasts.view==Podcasts::View::Channels && requests==0);
  service();tick();
  key(Key::Center);assert(podcasts.busy);service();tick();
  assert(podcasts.total==30 && podcasts.page["episodes"].size()==12);
  for (int i=0;i<13;++i) key(Key::Down);
  service();tick();assert(podcasts.selected==13 && podcasts.page["offset"]==12);
  key(Key::Center);service();tick();
  assert(podcasts.view==Podcasts::View::Playback && podcasts.position()==15 && playedUrl=="https://example.org/13.mp3");
  uint32_t rate=0;radio.read(nullptr,10,rate);
  key(Key::Center);assert(radio.paused());service();
  assert(podcasts.position()==25);
  key(Key::Center);assert(!radio.paused());
  key(Key::Right);assert(podcasts.position()==55);
  key(Key::Left);assert(podcasts.position()==40);
  // Relative playback navigation uses the stable episode anchor, then starts immediately.
  key(Key::Down);service();tick();service();tick();assert(playedUrl=="https://example.org/14.mp3");
  key(Key::Up);service();tick();service();tick();assert(playedUrl=="https://example.org/13.mp3" && podcasts.position()==40);
  unsigned plays=playCount;
  key(Key::Left,true);service();tick();
  assert(podcasts.view==Podcasts::View::Episodes && radio.active() && playCount==plays);
  key(Key::Left);assert(podcasts.view==Podcasts::View::Channels && radio.active());
  key(Key::Left);assert(podcasts.view==Podcasts::View::Playback);
  // Page exit silences immediately, while persistence stays on the network worker.
  online=false;podcasts.show(false,config["podcasts"].as<JsonArrayConst>());
  assert(!radio.active());service();
  assert(files.count("/podcast-progress.json"));
  // A fresh inbox restores persisted last metadata/position without the server.
  PodcastInbox reboot;assert(reboot.begin());
  reboot.request("/podcasts/resume",PodcastRequestKind::Resume);reboot.service(net,"/api/v1/devices/child",false);
  std::unique_ptr<PodcastReply> restored(reboot.take());
  assert(restored && restored->cached && restored->data["episode"]["position"]==40);
  // Known episodes can start from cached page metadata, including unseen details.
  tick();service();tick();assert(podcasts.view==Podcasts::View::Playback && podcasts.position()==40);
  key(Key::Left,true);service();tick();key(Key::Down); // Episode 14 was in the page cache.
  key(Key::Center);service();tick();assert(playedUrl=="https://example.org/14.mp3");
  fakeNow+=20000;radio.read(nullptr,20,rate);tick();service();
  // Revocation invalidates playback synchronously without waiting for HTTP.
  config["podcasts"].clear();tick();assert(!radio.active() && podcasts.view==Podcasts::View::Channels);
  // Reconnection uploads journal revisions idempotently.
  fakeNow+=30000;online=true;service();assert(uploads>0 && saved["revision"].as<uint64_t>()>0);
  // Natural completion is journaled before auto-next; switching must retain completed status.
  config["podcasts"][0]["id"]=1;
  podcasts.show(false,config["podcasts"].as<JsonArrayConst>());tick();service();tick();
  assert(radio.active() && playedUrl=="https://example.org/14.mp3");
  eof=true;radio.read(nullptr,0,rate);eof=false;
  tick();service();tick();
  assert(playedUrl=="https://example.org/15.mp3" && radio.active());
  service();
  JsonDocument journal;deserializeJson(journal,files["/podcast-progress.json"]);
  bool completed=false;
  for (JsonObject row:journal["rows"].as<JsonArray>())
    if (row["episodeId"].as<String>()==String(std::string(63,'0')+'e')) completed=row["status"]=="completed";
  assert(completed);
  // A stale reply from a prior page visit must not restart audio after exit.
  config["podcasts"][0]["id"]=1;
  podcasts.show(false,config["podcasts"].as<JsonArrayConst>());tick();service();
  podcasts.show(false,config["podcasts"].as<JsonArrayConst>());podcasts.tick();assert(!radio.active());
  puts("PASS: Podcast views, 30-episode pagination, partial resume, seek, pause, uninterrupted browsing, page exit, reboot cache, offline selection, uploads, revocation and stale replies");
}
