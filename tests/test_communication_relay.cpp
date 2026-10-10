#include "Communication.h"
#include "Assets.h"
#include "Transport.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) { return malloc(n); }
void RamAllocator::deallocate(void *p) { free(p); }
void *RamAllocator::reallocate(void *p, size_t n) { return realloc(p,n); }
DeviceSettings deviceSettings;
Storage storage;
Assets assets;
bool Assets::definition(const String &, int, JsonDocument &out) { out["messagesFile"] = "messages.json"; return true; }
String Assets::resolve(const String &, int, const String &path) { return path; }
bool Storage::readJson(const String &, JsonDocument &out, size_t) {
  out["schemaVersion"] = 1;
  auto rows = out["messages"].to<JsonArray>();
  auto row = rows.add<JsonObject>(); row["id"] = "template"; row["text"] = "Hallo";
  return true;
}
String Transport::encode(const String &s) { return s; }
bool Transport::json(const String &, JsonDocument &, JsonDocument *) { return false; }
}
struct Server {
  int status = 200, posts = 0;
  bool losePostResponse = false, deny = false, invalid = false;
  std::vector<bool> mine;
  std::map<std::string,uint64_t> accepted;
  std::function<void()> onGet;
  void add(bool own = false) { mine.push_back(own); }
  bool json(const String &path, JsonDocument &out, JsonDocument *body) {
    status = deny ? 403 : 200;
    if (deny) return false;
    if (body) {
      ++posts;
      std::string event = (*body)["eventId"].as<std::string>();
      assert(event.size() == 32 && (*body)["templateId"] == "template");
      if (!accepted.count(event)) { add(true); accepted[event] = mine.size(); }
      if (losePostResponse) { losePostResponse = false; status = 502; return false; }
      out["ok"] = true; out["eventId"] = event;
      return true;
    }
    if (onGet) { auto action = onGet; onGet = {}; action(); }
    auto query = path.find("?since=");
    bool initial = query == std::string::npos;
    uint64_t since = initial ? 0 : std::stoull(path.substr(query + 7));
    bool reset = since > mine.size();
    size_t start = initial || reset ? (mine.size() > 8 ? mine.size()-8 : 0) : since;
    size_t end = std::min(mine.size(), start+8);
    out["schemaVersion"] = invalid ? 99 : 1;
    out["reset"] = reset; out["more"] = end < mine.size(); out["cursor"] = end;
    auto rows = out["messages"].to<JsonArray>();
    for (size_t i = start; i < end; ++i) {
      auto row = rows.add<JsonObject>();
      row["id"] = uint64_t(i+1); row["senderId"] = mine[i] ? "local" : "remote";
      row["name"] = "Server name"; row["text"] = "Server text"; row["symbol"] = "👋";
    }
    return true;
  }
};
static JsonDocument config(bool enabled = true) {
  JsonDocument state;
  state["config"]["communicationEnabled"] = enabled;
  state["assets"]["communication-messages"] = 1;
  return state;
}
static void step(Communication &c, Server &s, uint32_t time, bool wifi = true) {
  fakeNow = time;
  c.service(time,wifi,[&](const String &path, JsonDocument &out, JsonDocument *body) { return s.json(path,out,body); },[&] { return s.status; });
}
int main() {
  strcpy(deviceSettings.deviceId,"local");
  auto state = config();
  Communication c;
  c.configure(state); assert(c.begin());
  Server s;
  s.add(); s.add(true);
  step(c,s,1000);
  assert(!c.poll() && c.count==2 && !c.unread); // Historical boot seed.
  s.add(); s.add(true);
  step(c,s,3000);
  assert(c.poll() && c.unread && c.count==4); // Notify incoming, never own echo.
  assert(!c.poll()); c.markRead(); assert(!c.unread);
  step(c,s,5000); assert(!c.poll()); // No duplicate notification on a repeated fetch.
  step(c,s,7000,false); assert(!c.online);
  s.add();
  step(c,s,9000); assert(c.poll() && c.unread); // Reconnect catches messages since cursor.
  c.markRead();
  s.losePostResponse = true;
  assert(c.send(0));
  step(c,s,9010);
  assert(c.sendStatus == RelaySendStatus::Retrying && !c.poll() && !c.unread);
  assert(s.accepted.size()==1 && s.posts==1);
  step(c,s,10000); assert(s.posts==1);
  s.add(); // Intervening remote message must survive a POST acknowledgement.
  step(c,s,11010);
  assert(c.sendStatus == RelaySendStatus::Sent && s.posts==2 && s.accepted.size()==1);
  assert(c.poll() && c.unread);
  c.markRead();
  s.add(); s.invalid=true;
  step(c,s,13010); assert(!c.poll() && !c.online);
  s.invalid=false;
  step(c,s,18010); assert(c.poll() && c.unread); // Malformed page never advances the cursor.
  auto disabled = config(false);
  c.configure(disabled); assert(!c.unread && c.count==0);
  assert(!c.send(0));
  c.configure(state);
  step(c,s,19010); assert(!c.poll() && c.count==8 && !c.unread);
  s.deny=true; assert(c.send(0));
  step(c,s,21010); assert(c.sendStatus == RelaySendStatus::Rejected && !c.online);
  int sent = s.posts;
  step(c,s,26010); assert(s.posts==sent); // Terminal permission error isn't retried.
  s.deny=false;
  s.onGet=[&] { c.configure(disabled); };
  step(c,s,31010); assert(!c.poll() && !c.unread && c.count==0); // In-flight disable discards stale responses.

  Communication blocked;
  blocked.configure(state); assert(blocked.begin());
  Server many;
  step(blocked,many,1000); // One seed-control record waits in the queue.
  for (int i=0;i<24;++i) many.add();
  step(blocked,many,3000); // First batch fits.
  step(blocked,many,3030); // Next batch would overflow: cursor stays at eight.
  assert(blocked.poll() && blocked.count==8);
  step(blocked,many,8030); assert(blocked.poll() && blocked.history[7].id==16);
  step(blocked,many,8060); assert(blocked.poll() && blocked.history[7].id==24);
  blocked.markRead(); step(blocked,many,10060); assert(!blocked.poll() && !blocked.unread);
  // Future cursor after a server restore seeds silently instead of losing future events.
  many.mine.resize(3);
  step(blocked,many,12060); assert(!blocked.poll() && blocked.count==3 && !blocked.unread);
  many.add(); step(blocked,many,14060); assert(blocked.poll());
  puts("PASS: real relay worker, boot history, own echoes, unread/read, reconnect, idempotent retries, malformed pages, permission changes, queue backpressure and restore");
}
