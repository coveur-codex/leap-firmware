#include "Podcasts.h"
#include "Transport.h"
#include "PodcastPlayback.h"
#include "PodcastProtocol.h"
#include <memory>
#include <time.h>
#include <vector>
#include <set>
namespace leap {
PodcastInbox podcastInbox;
namespace {
String cachePath(const String &key) {
  uint32_t hash=2166136261u;
  for (size_t i=0; i<key.length(); ++i) hash=(hash^uint8_t(key[i]))*16777619u;
  return String("/podcast-cache/")+String(hash%32)+".json";
}
bool granted(JsonVariantConst config, int id) {
  bool enabled=false;
  for (JsonObjectConst p: config["pages"].as<JsonArrayConst>())
    if (p["id"]=="podcasts") enabled=p["enabled"]|false;
  for (JsonObjectConst p: config["podcasts"].as<JsonArrayConst>())
    if (enabled && p["id"]==id) return true;
  return false;
}
}
bool PodcastInbox::begin() {
  updates=xQueueCreate(32,sizeof(JsonDocument *));
  return updates;
}
uint32_t PodcastInbox::request(const String &suffix, PodcastRequestKind kind) {
  uint32_t id=++serial;
  if (!id) id=++serial;
  delete pending.exchange(new Request{suffix,id,kind});
  return id;
}
bool PodcastInbox::save(JsonVariantConst episode, uint32_t position, uint32_t duration, bool completed) {
  if (!updates || episode["id"].isNull()) return false;
  auto *update=new JsonDocument(&jsonRam);
  (*update)["episode"]=episode;
  (*update)["position"]=position;
  (*update)["duration"]=duration;
  (*update)["completed"]=completed;
  if (xQueueSend(updates,&update,0)==pdTRUE) return true;
  delete update;
  storageError=true;
  return false;
}
void PodcastInbox::overlay(JsonVariant episode) {
  if (episode.isNull()) return;
  for (JsonObject row: journal["rows"].as<JsonArray>())
    if (row["episodeId"]==episode["id"] && row["revision"].as<uint64_t>()>=episode["revision"].as<uint64_t>()) {
      episode["position"]=row["position"];
      episode["status"]=row["status"];
      if (row["duration"].as<uint32_t>()) episode["duration"]=row["duration"];
      episode["revision"]=row["revision"];
      break;
    }
}
bool PodcastInbox::restore(const String &path, JsonDocument &out) {
  JsonDocument cached(&jsonRam);
  if (storage.readJson(cachePath(path),cached,65536) && cached["key"]==path) {
    out.set(cached["data"]); return true;
  }
  return false;
}
void PodcastInbox::cache(const String &path, JsonDocument &value) {
  JsonDocument old(&jsonRam), entry(&jsonRam);
  // Avoid flash writes for identical responses and revisiting cached lists.
  if (restore(path,old) && old==value) return;
  entry["key"]=path; entry["data"]=value;
  if (storage.freeBytes()<ReserveBytes+2*measureJson(entry)) return;
  if (!storage.writeJson(cachePath(path),entry,65536)) log("PODCAST", "Metadata cache write deferred");
}
void PodcastInbox::service(Transport &net, const String &base, bool connected) {
  if (!updates) return;
  if (!loaded) {
    storage.readJson("/podcast-progress.json",journal,196608);
    if (!journal["rows"].is<JsonArray>()) journal["rows"].to<JsonArray>();
    loaded=true;
  }
  JsonDocument *raw=nullptr;
  while (xQueueReceive(updates,&raw,0)==pdTRUE) {
    std::unique_ptr<JsonDocument> update(raw);
    auto episode=(*update)["episode"];
    auto rows=journal["rows"].as<JsonArray>();
    JsonObject row;
    for (JsonObject candidate: rows) if (candidate["episodeId"]==episode["id"]) { row=candidate; break; }
    if (row.isNull()) {
      if (rows.size()>=128) {
        int index=0, remove=-1;
        for (JsonObject candidate: rows) { if (!(candidate["pending"]|false)) { remove=index; break; } ++index; }
        if (remove>=0) rows.remove(remove);
      }
      if (rows.size()>=128) {
        // Preserve every unsent record. Put this change back for a later retry.
        auto *retry=update.release();
        if (xQueueSend(updates,&retry,0)!=pdTRUE) delete retry;
        storageError=true; break;
      }
      row=rows.add<JsonObject>();
    }
    uint64_t stamp=uint64_t(std::max(time_t(0),time(nullptr)))*1000;
    uint64_t revision=std::max({stamp,journal["clock"].as<uint64_t>(),episode["revision"].as<uint64_t>()})+1;
    journal["clock"]=revision;
    row["episodeId"]=episode["id"]; row["podcastId"]=episode["podcastId"];
    row["position"]=(*update)["position"]; row["duration"]=(*update)["duration"];
    row["status"]=(*update)["completed"]==true ? "completed" : "partial";
    row["naturalEnd"]=(*update)["completed"]==true;
    row["revision"]=revision; row["pending"]=true;
    journal["last"]=episode;
    overlay(journal["last"]);
    dirty=true;
  }
  // Persist before upload: reboot after HTTP success may safely repeat the revision.
  if (dirty && storage.writeJson("/podcast-progress.json",journal,196608)) { dirty=false; storageError=false; }
  else if (dirty) storageError=true;
  if (connected && !dirty && (pending.load() || elapsed(millis(),lastRetry,30000))) {
    lastRetry=millis();
    unsigned sent=0;
    auto upload=[&](JsonObject row) {
      JsonDocument body(&jsonRam), response(&jsonRam);
      body.set(row); body.remove("pending");
      bool ok=net.json(base+"/podcasts/progress",response,&body,8000) && response["ok"]==true;
      if (!ok && net.status!=404) return false;
      // A revoked/deleted episode must not keep the retry queue blocked.
      row["pending"]=false; ++sent;
      return true;
    };
    bool proceed=true;
    for (JsonObject row:journal["rows"].as<JsonArray>())
      if ((row["pending"]|false) && row["episodeId"]==journal["last"]["id"]) { proceed=upload(row); break; }
    if (proceed) for (JsonObject row:journal["rows"].as<JsonArray>()) {
      if (!(row["pending"]|false)) continue;
      if (!upload(row) || sent>=8) break;
    }
  }
  std::unique_ptr<Request> request(pending.exchange(nullptr));
  if (!request) return;
  auto reply=std::unique_ptr<PodcastReply>(new PodcastReply);
  reply->ticket=request->ticket; reply->kind=request->kind;
  JsonDocument snapshot(&jsonRam);
  if (!storage.load(snapshot)) { reply->http=0; delete ready.exchange(reply.release()); return; }
  auto config=snapshot["config"];
  bool isResume=request->kind==PodcastRequestKind::Resume;
  int podcastId=0;
  if (!isResume) {
    // Every path is generated by the UI, no arbitrary external endpoints.
    sscanf(request->path.c_str(),"/podcasts/%d/",&podcastId);
    if (!granted(config,podcastId)) { reply->http=404; delete ready.exchange(reply.release()); return; }
  }
  bool success=connected && net.json(base+request->path,reply->data,nullptr,8000);
  reply->http=connected ? net.status : 0;
  auto valid=[&] { return request->kind==PodcastRequestKind::List ? podcastPage(reply->data,podcastId) : podcastSingle(reply->data,podcastId); };
  if (success && !valid()) { success=false; reply->http=502; }
  if (success) {
    if (isResume && !journal["last"].isNull() &&
        journal["last"]["revision"].as<uint64_t>()>reply->data["episode"]["revision"].as<uint64_t>()) {
      // Validate a newer local last episode before resuming it online.
      auto last=journal["last"];
      JsonDocument fresh(&jsonRam);
      if (net.json(base+"/podcasts/"+String(last["podcastId"]|0)+"/episodes/"+last["id"].as<String>(),fresh,nullptr,8000))
        reply->data.set(fresh);
    }
    if (isResume && reply->data["episode"].isNull()) { journal.remove("last"); dirty=true; }
    // Persist bounded metadata pages. Full audio files are never cached.
    if (request->kind!=PodcastRequestKind::Next) cache(request->path,reply->data);
  } else if (!connected || reply->http<=0 || reply->http>=500) {
    if (isResume && !journal["last"].isNull()) {
      reply->data["episode"]=journal["last"]; reply->data["offset"]=journal["last"]["index"]|0;
      reply->cached=true;
    } else {
      reply->cached=restore(request->path,reply->data);
      int anchor=request->path.indexOf("&anchor=");
      if (!reply->cached && request->kind==PodcastRequestKind::List && anchor>=0)
        reply->cached=restore(request->path.substring(0,anchor),reply->data);
    }
    if (!reply->cached && request->kind==PodcastRequestKind::Episode) {
      String wanted=request->path.substring(request->path.length()-64);
      for (int i=0;i<32 && !reply->cached;++i) {
        JsonDocument cached(&jsonRam);
        if (!storage.readJson(String("/podcast-cache/")+String(i)+".json",cached,65536)) continue;
        JsonVariant data=cached["data"];
        if (data["podcastId"]!=podcastId) continue;
        int index=data["offset"]|0;
        JsonArray entries=data["episodes"].as<JsonArray>();
        for (JsonVariant e:entries) {
          if (e["id"]==wanted) { reply->data["episode"]=e; reply->data["offset"]=index; reply->cached=true; break; }
          ++index;
        }
      }
    }
    if (request->kind==PodcastRequestKind::Next) {
      // Offline next: search the bounded disk cache; only one page is in RAM at a time.
      JsonDocument best(&jsonRam);
      for (int i=0;i<32;++i) {
        JsonDocument cached(&jsonRam);
        if (!storage.readJson(String("/podcast-cache/")+String(i)+".json",cached,65536)) continue;
        JsonVariant data=cached["data"];
        if (data["podcastId"]!=podcastId) continue;
        int index=data["offset"]|0;
        JsonArray entries=data["episodes"].as<JsonArray>();
        for (JsonVariant e:entries) {
          overlay(e);
          if (e["status"]!="completed" && (best.isNull() || (e["publishedAt"].as<String>()>best["episode"]["publishedAt"].as<String>()))) {
            best["episode"]=e; best["offset"]=index;
          }
          ++index;
        }
      }
      reply->data.set(best); reply->cached=!best.isNull();
    }
  }
  overlay(reply->data["episode"]);
  if (success && request->kind==PodcastRequestKind::Next) {
    std::set<std::string> skipped;
    while (reply->data["episode"]["status"]=="completed") {
      String id=reply->data["episode"]["id"].as<String>();
      if (!skipped.insert(id.c_str()).second || skipped.size()>128 || pending.load()) { reply->data["episode"]=nullptr; break; }
      JsonDocument next(&jsonRam);
      if (!net.json(base+"/podcasts/"+String(podcastId)+"/next?after="+id,next,nullptr,8000)) {
        reply->data.clear();reply->http=net.status;break;
      }
      reply->data.set(next);overlay(reply->data["episode"]);
    }
  }
  for (JsonVariant e: reply->data["episodes"].as<JsonArray>()) overlay(e);
  if (!reply->data["episode"].isNull() && !granted(config,reply->data["episode"]["podcastId"]|0)) {
    reply->data.clear(); reply->http=404; reply->cached=false;
  }
  if ((!reply->data.isNull() && !valid()) || reply->data.overflowed()) {
    reply->data.clear();reply->http=502;reply->cached=false;
  }
  // Recheck latest config after HTTP; revocations cannot resurrect stale work.
  JsonDocument latest(&jsonRam);
  if (storage.load(latest) && !isResume && !granted(latest["config"],podcastId)) {
    reply->data.clear(); reply->http=404; reply->cached=false;
  }
  delete ready.exchange(reply.release());
}
} // namespace leap
