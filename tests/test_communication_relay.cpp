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

static JsonDocument config(bool enabled = true) {
  JsonDocument state;
  state["config"]["communicationEnabled"] = enabled;
  state["assets"]["communication-messages"] = 1;
  return state;
}
static void step(Communication &c, uint32_t time, bool wifi=true) {
  fakeNow=time; c.service(time,wifi);
}
static void event(Communication &c, WebSocketsClient &socket, WStype_t type, const std::string &frame, uint32_t time) {
  socket.events.push_back({type,frame}); step(c,time);
}
static std::string page(uint64_t start, unsigned count, bool reset=false, bool more=false) {
  JsonDocument doc;
  doc["type"]="messages"; doc["schemaVersion"]=1;
  doc["cursor"]=start+count; doc["reset"]=reset; doc["more"]=more;
  auto rows=doc["messages"].to<JsonArray>();
  for (unsigned i=0;i<count;++i) {
    auto row=rows.add<JsonObject>(); row["id"]=start+i+1;
    row["senderId"]=i%2 ? "local" : "remote";
    row["name"]="Name"; row["text"]="Hallo"; row["symbol"]="!";
  }
  std::string raw; serializeJson(doc,raw); return raw;
}
static JsonDocument last(WebSocketsClient &socket) {
  JsonDocument doc; assert(!socket.sent.empty());
  assert(!deserializeJson(doc,socket.sent.back())); return doc;
}
int main() {
  strcpy(deviceSettings.deviceId,"local");
  strcpy(deviceSettings.server,"http://homeserver:8080");
  auto state=config();
  Communication c; c.configure(state); assert(c.begin());
  auto &s=*WebSocketsClient::instances.back();
  step(c,1000);
  assert(s.host=="homeserver" && s.port==8080 && s.url=="/api/v1/devices/local/communication/ws");
  assert(s.heartbeat==30000);
  event(c,s,WStype_CONNECTED,"",1010);
  assert(last(s)["type"]=="sync" && !last(s)["since"].is<uint64_t>());
  event(c,s,WStype_TEXT,page(0,2),1020);
  assert(c.online && !c.poll() && c.count==2 && !c.unread); // Silent boot.
  assert(last(s)["since"]==2);
  auto sent=s.sent.size(); step(c,15000); assert(s.sent.size()==sent); // No idle polling.
  event(c,s,WStype_TEXT,page(2,2),15010);
  assert(c.poll() && c.unread); c.markRead();
  event(c,s,WStype_TEXT,page(4,0),15020); assert(!c.poll());
  event(c,s,WStype_DISCONNECTED,"",16000);
  assert(!c.online && s.interval==2000);
  event(c,s,WStype_CONNECTED,"",18000); assert(last(s)["since"]==4);
  event(c,s,WStype_TEXT,page(4,1),18010); assert(c.poll()); c.markRead();
  assert(c.send(0)); step(c,19000);
  auto request=last(s); assert(request["type"]=="send");
  auto id=request["eventId"].as<std::string>(); assert(id.size()==32);
  step(c,29000); assert(!s.connected && c.sendStatus==RelaySendStatus::Retrying);
  event(c,s,WStype_CONNECTED,"",31000);
  assert(last(s)["eventId"].as<std::string>()==id); // Lost ack retries same ID.
  std::string ack="{\"type\":\"ack\",\"ok\":true,\"eventId\":\""+id+"\"}";
  event(c,s,WStype_TEXT,ack,31010); assert(c.sendStatus==RelaySendStatus::Sent);
  event(c,s,WStype_TEXT,"{\"type\":\"messages\",\"schemaVersion\":99}",32000);
  assert(!c.online && !s.connected);
  event(c,s,WStype_CONNECTED,"",34000); assert(last(s)["since"]==5);
  event(c,s,WStype_TEXT,page(5,1),34010); assert(c.poll()); c.markRead();
  assert(c.sendIcon(20)); step(c,35000); assert(last(s)["templateId"]=="icon:help");
  assert(!c.sendIcon(0));
  id=last(s)["eventId"].as<std::string>();
  event(c,s,WStype_TEXT,"{\"type\":\"error\",\"status\":422,\"eventId\":\""+id+"\"}",35010);
  assert(c.sendStatus==RelaySendStatus::Rejected);
  auto disabled=config(false); c.configure(disabled); step(c,36000);
  assert(!s.connected && !c.unread && c.count==0 && !c.send(0));
  c.configure(state); step(c,38000); event(c,s,WStype_CONNECTED,"",38010);
  event(c,s,WStype_TEXT,page(0,8),38020); assert(!c.poll() && c.count==8);
  // UI stalls: first batch fits, second must not advance cursor.
  event(c,s,WStype_TEXT,page(8,8),39000);
  event(c,s,WStype_TEXT,page(16,8),39010);
  event(c,s,WStype_TEXT,page(24,8),39020);
  assert(!s.connected); assert(c.poll() && c.history[7].id==24);
  event(c,s,WStype_CONNECTED,"",41000); assert(last(s)["since"]==24);
  event(c,s,WStype_TEXT,page(24,8),41010); assert(c.poll() && c.history[7].id==32);
  c.markRead();
  event(c,s,WStype_TEXT,page(0,3,true),42000); assert(!c.poll() && c.count==3 && !c.unread);
  sent=s.sent.size(); step(c,72000); assert(s.sent.size()==sent);
  step(c,73000,false); assert(!s.connected && !c.online);
  s.failConnect=true;
  for (unsigned i=0;i<6;++i) {
    step(c,75000+i*30000);
    assert(s.interval==std::min(2000u << i,30000u)); // TCP failures have no callback.
  }
  Communication secure; secure.configure(state); assert(secure.begin());
  auto &tls=*WebSocketsClient::instances.back();
  strcpy(deviceSettings.server,"https://homeserver/prefix");
  step(secure,300000); assert(!tls.started); // Never fall back to insecure WSS.
  strcpy(deviceSettings.tlsCa,"test-ca");
  step(secure,301000);
  assert(tls.started && tls.ca=="test-ca" && tls.port==443 &&
         tls.url=="/prefix/api/v1/devices/local/communication/ws");
  puts("PASS: WebSocket push, no idle polls, keepalive, cursor resume, idempotent sends, rejection, backpressure and restore");
}
