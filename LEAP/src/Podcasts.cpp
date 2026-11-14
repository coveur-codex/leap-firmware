#include "Podcasts.h"
#include "Audio.h"
#include "Radio.h"
#include "PodcastPlayback.h"
#include <memory>
namespace leap {
bool Podcasts::allowed(int id) const {
  for (JsonObjectConst p:channels.as<JsonArrayConst>()) if (p["id"]==id) return true;
  return false;
}
String Podcasts::channelName(int id) const {
  for (JsonObjectConst p:channels.as<JsonArrayConst>()) if (p["id"]==id) return p["name"].as<String>();
  return "Podcasts";
}
void Podcasts::request(const String &suffix, PodcastRequestKind kind) {
  ticket=podcastInbox.request(suffix,kind); waiting=kind; busy=true;
}
void Podcasts::loadList(int index, bool start) {
  if (!channels.size()) return;
  channel=constrain(channel,0,int(channels.size())-1);
  wantedIndex=std::max(0,index); startAfterList=start;
  int id=channels[channel]["id"]|0;
  String path=String("/podcasts/")+String(id)+"/episodes?offset="+String((wantedIndex/12)*12);
  if (start && current["podcastId"]==id)
    path+="&anchor="+current["id"].as<String>()+"&direction="+String(wantedIndex-currentIndex);
  request(path,PodcastRequestKind::List);
}
uint32_t Podcasts::position() const { return playing ? radio.position() : current["position"].as<uint32_t>(); }
uint32_t Podcasts::duration() const { return playing && radio.duration() ? radio.duration() : current["duration"].as<uint32_t>(); }
void Podcasts::save(bool completed) {
  if (current["id"].isNull() || !playing) return;
  completed=completed || (ended && current["status"]=="completed");
  if (savedId==current["id"].as<String>() && savedPosition==position() && savedDuration==duration() && savedCompleted==completed) {
    lastSaved=millis(); return;
  }
  current["position"]=position(); current["duration"]=duration();
  current["status"]=completed ? "completed" : "partial";
  current["index"]=currentIndex;
  if (podcastInbox.save(current.as<JsonVariantConst>(),position(),duration(),completed)) {
    lastSaved=millis(); savedPosition=position(); savedDuration=duration();
    savedId=current["id"].as<String>(); savedCompleted=completed;
  }
  else message="Fortschritt wartet auf Speicher";
}
void Podcasts::start(JsonVariantConst episode, int index) {
  if (!allowed(episode["podcastId"]|0)) { message="Podcast nicht freigegeben"; return; }
  if (playing) save();
  JsonDocument replacement(&jsonRam); replacement.set(episode);
  current.set(replacement); currentIndex=index;
  uint32_t startAt=current["position"].as<uint32_t>(), length=current["duration"]|0;
  // A prior forced seek can leave a partial record exactly at EOF; replay it from zero.
  if (current["status"]=="completed" || (length && startAt>=length)) startAt=0;
  current["position"]=startAt; current["status"]="partial";
  audio.music(MusicProfile::Off); audio.stop();
  radio.playEpisode(current["url"].as<String>(),startAt,current["duration"]|0);
  view=View::Playback; playing=true; ended=false;
  lastSaved=millis(); message="";
  save();
}
void Podcasts::show(bool shown, JsonArrayConst granted) {
  bool changed=channels!=granted;
  channels.set(granted);
  if (changed) {
    if (!current["id"].isNull() && !allowed(current["podcastId"]|0)) {
      if (playing) { save(); radio.stop(); }
      playing=false; current.clear(); view=View::Channels; busy=false; ticket=0;
      message="Podcast nicht mehr freigegeben";
    }
    if (!page["podcastId"].isNull() && !allowed(page["podcastId"]|0)) { page.clear(); view=View::Channels; busy=false; ticket=0; }
    channel=channels.size() ? constrain(channel,0,int(channels.size())-1) : 0;
  }
  if (!shown && visible) {
    if (playing) { save(); radio.stop(); }
    playing=false; busy=false; ticket=0;
  }
  if (shown && !visible) {
    view=View::Channels; message="";
    request("/podcasts/resume",PodcastRequestKind::Resume);
  }
  visible=shown;
  if (visible) tick();
}
void Podcasts::tick() {
  if (auto *raw=podcastInbox.take()) {
    std::unique_ptr<PodcastReply> reply(raw);
    if (visible && reply->ticket==ticket) {
      busy=false;
      bool success=(reply->http>=200 && reply->http<300) || reply->cached;
      if (!success) message=reply->http==404 ? "Episode nicht mehr verfügbar" : "Homeserver nicht erreichbar";
      else if (reply->kind==PodcastRequestKind::List) {
        if (reply->data["episodes"].is<JsonArray>()) {
          page.set(reply->data); total=page["total"]|0;
          if (page["selectedOffset"].is<int>()) wantedIndex=page["selectedOffset"].as<int>();
          selected=total ? constrain(wantedIndex,0,total-1) : 0;
          if (selected < (page["offset"]|0) && total) loadList(selected,startAfterList);
          else if (startAfterList) {
            int local=selected-(page["offset"]|0);
            if (local>=0 && local<int(page["episodes"].size())) {
              auto e=page["episodes"][local];
              request(String("/podcasts/")+String(e["podcastId"]|0)+"/episodes/"+e["id"].as<String>(),PodcastRequestKind::Episode);
            }
          }
          if (!total) message="Keine MP3-Episoden";
          else if (reply->cached) message="Gespeicherte Episoden";
          else message="";
        } else message="Episoden konnten nicht geladen werden";
      } else {
        auto e=reply->data["episode"];
        if (!e.isNull() && allowed(e["podcastId"]|0)) {
          for (size_t i=0;i<channels.size();++i) if (channels[i]["id"]==e["podcastId"]) channel=int(i);
          if ((reply->kind==PodcastRequestKind::Resume || reply->kind==PodcastRequestKind::Next) && e["status"]=="completed") {
            if (reply->kind==PodcastRequestKind::Resume) {
              current.set(e); currentIndex=reply->data["offset"]|0;
              request(String("/podcasts/")+String(e["podcastId"]|0)+"/next?after="+e["id"].as<String>(),PodcastRequestKind::Next);
            } else message="Keine weitere ungehörte Folge im Cache";
          } else start(e,reply->data["offset"]|0);
        } else if (reply->kind==PodcastRequestKind::Next) message="Alle Folgen gehört";
      }
    }
  }
  if (!visible || !playing) return;
  if (message=="Spulen …" && radio.status()==RadioStatus::Playing) message="";
  if (radio.isSuspended()) { save(); playing=false; return; }
  if (!radio.paused() && elapsed(millis(),lastSaved,20000) && !ended) save();
  if (radio.status()==RadioStatus::Ended && !ended) {
    ended=true;
    bool natural=radio.naturalEnd();
    save(natural);
    if (natural) request(String("/podcasts/")+String(current["podcastId"]|0)+"/next?after="+current["id"].as<String>(),PodcastRequestKind::Next);
    else message="Folge beendet · Sprung ans Ende";
  }
}
void Podcasts::action(const InputEvent &event) {
  if (!visible) return;
  if (event.longPress) {
    if (view==View::Playback && event.key==Key::Left) {
      view=View::Episodes;
      for (size_t i=0;i<channels.size();++i) if (channels[i]["id"]==current["podcastId"]) channel=int(i);
      selected=currentIndex; loadList(selected);
    }
    return;
  }
  int direction=(event.key==Key::Down)-(event.key==Key::Up);
  if (view==View::Channels) {
    if (direction && channels.size()) { ticket=0; busy=false; channel=constrain(channel+direction,0,int(channels.size())-1); }
    if ((event.key==Key::Right || event.key==Key::Center) && channels.size()) {
      view=View::Episodes; selected=0; total=0; page.clear(); loadList(0);
    }
    if (event.key==Key::Left && !current["id"].isNull() && allowed(current["podcastId"]|0)) {
      ticket=0; busy=false; view=View::Playback;
    }
  } else if (view==View::Episodes) {
    if (event.key==Key::Left) { view=View::Channels; ticket=0; busy=false; return; }
    if (direction && total) {
      selected=constrain(selected+direction,0,total-1);
      int offset=page["offset"]|0;
      if (selected<offset || selected>=offset+int(page["episodes"].size())) loadList(selected);
    }
    if ((event.key==Key::Right || event.key==Key::Center) && !busy) {
      int local=selected-(page["offset"]|0);
      if (local>=0 && local<int(page["episodes"].size())) {
        auto e=page["episodes"][local];
        // Revalidate metadata/URL on manual selection; offline uses this cached page.
        request(String("/podcasts/")+String(e["podcastId"]|0)+"/episodes/"+e["id"].as<String>(),PodcastRequestKind::Episode);
      }
    }
  } else if (!current["id"].isNull()) {
    if (direction) {
      for (size_t i=0;i<channels.size();++i) if (channels[i]["id"]==current["podcastId"]) channel=int(i);
      loadList(std::max(0,(busy && startAfterList ? wantedIndex : currentIndex)+direction),true);
    } else if (event.key==Key::Left || event.key==Key::Right) {
      if (playing) save();
      uint32_t target=podcastSeek(position(),event.key==Key::Left ? -15 : 30,duration());
      current["position"]=target;
      radio.playEpisode(current["url"].as<String>(),target,duration(),true);
      playing=true; ended=false; message="Spulen …";
    } else if (event.key==Key::Center) {
      if (!playing || !radio.active()) {
        if (playing) save();
        request(String("/podcasts/")+String(current["podcastId"]|0)+"/episodes/"+current["id"].as<String>(),PodcastRequestKind::Episode);
        return;
      }
      radio.pause(!radio.paused()); save(); message="";
    }
  }
}
} // namespace leap
