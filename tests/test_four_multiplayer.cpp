#include "FourMultiplayer.h"
#include "Communication.h"
#include "Transport.h"
#include <cassert>
#include <iostream>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) { return malloc(n); }
void RamAllocator::deallocate(void *p) { free(p); }
void *RamAllocator::reallocate(void *p,size_t n) { return realloc(p,n); }
DeviceSettings deviceSettings;
String Transport::encode(const String &s) { return s; }
}
static JsonDocument state(unsigned move=0,unsigned revision=1,unsigned round=1) {
  JsonDocument f;
  f["type"]="state"; f["schemaVersion"]=1; f["session"]=std::string(32,'a');
  f["revision"]=revision; f["remainingMs"]=30000; f["round"]=round; f["move"]=move;
  f["phase"]="playing"; f["turn"]=1; f["winner"]=0; f["reason"]="";
  auto players=f["players"].to<JsonArray>();
  for (const char *id:{"local","remote"}) {
    auto p=players.add<JsonObject>();
    p["id"]=id; p["name"]="Änne"; p["accentColor"]="#ff9933";
    p["connected"]=true; p["seq"]=0; p["rematch"]=false; p["left"]=false;
  }
  auto board=f["board"].to<JsonArray>();
  for (int r=0;r<6;++r) { auto row=board.add<JsonArray>(); for (int c=0;c<7;++c) row.add(0); }
  return f;
}
static void step(unsigned now,bool wifi=true) { fakeNow=now; fourMultiplayer.service(now,wifi); }
static void deliver(WebSocketsClient &socket,JsonDocument &f,unsigned now) {
  std::string raw; serializeJson(f,raw); socket.events.push_back({WStype_TEXT,raw}); step(now);
}
static JsonDocument last(WebSocketsClient &socket) {
  JsonDocument f; assert(!socket.sent.empty()); assert(!deserializeJson(f,socket.sent.back())); return f;
}
int main() {
  FourSnapshot snapshot;
  auto valid=state(); assert(decodeFour(valid,snapshot) && snapshot.count==2);
  auto invalid=state(); invalid["players"].as<JsonArray>().add(valid["players"][0]); assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["players"][1]["id"]="local"; assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["remainingMs"]=30001; assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["board"][5][6]=3; assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["board"][5].as<JsonArray>().remove(0); assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["turn"]=0; assert(!decodeFour(invalid,snapshot));
  invalid=state(); invalid["move"]=43; assert(!decodeFour(invalid,snapshot));
  strcpy(deviceSettings.deviceId,"local"); strcpy(deviceSettings.server,"http://homeserver:8080");
  assert(fourMultiplayer.begin() && fourMultiplayer.start()); step(100);
  WebSocketsClient *socket=nullptr;
  for (auto s:WebSocketsClient::instances) if (s->url.find("/connect-four/ws")!=std::string::npos) socket=s;
  assert(socket && socket->interval==1000);
  socket->events.push_back({WStype_CONNECTED,{}}); step(200);
  assert(last(*socket)["type"]=="join" && last(*socket)["protocolVersion"]==1);
  socket->callbackDelay=1; deliver(*socket,valid,300); socket->callbackDelay=0;
  assert(fourMultiplayer.online && fourMultiplayer.poll(snapshot));
  fourMultiplayer.command(3,1,0); step(400);
  auto input=last(*socket);
  assert(input["type"]=="input" && input["column"]==3 && input["round"]==1 && input["move"]==0 && input["seq"]==1);
  auto newer=state(2,4); deliver(*socket,newer,450);
  auto older=state(1,3); deliver(*socket,older,460);
  assert(fourMultiplayer.poll(snapshot) && snapshot.move==2);
  fourMultiplayer.command(3,1,0); auto before=socket->sent.size(); step(470);
  assert(socket->sent.size()==before); // Stale move cannot be applied to a different turn.
  auto latest=state(3,5); deliver(*socket,newer,480); deliver(*socket,latest,490);
  assert(fourMultiplayer.poll(snapshot) && snapshot.move==3 && !fourMultiplayer.poll(snapshot));
  step(500,false); assert(!fourMultiplayer.online);
  step(600); socket->events.push_back({WStype_CONNECTED,{}}); step(700);
  assert(last(*socket)["session"]==std::string(32,'a'));
  latest["players"][0]["seq"]=7; latest["phase"]="finished"; latest["winner"]=1; latest["reason"]="four";
  deliver(*socket,latest,800);
  fourMultiplayer.command(0,1,3,true); step(900);
  assert(last(*socket)["type"]=="rematch" && last(*socket)["seq"]==8);
  auto next=state(0,6,2); deliver(*socket,next,950);
  assert(fourMultiplayer.poll(snapshot) && snapshot.round==2 && snapshot.move==0);
  fourMultiplayer.close(); fourMultiplayer.start(); step(960);
  assert(last(*socket)["type"]=="leave" && !fourMultiplayer.poll(snapshot));
  socket->events.push_back({WStype_CONNECTED,{}}); step(980);
  assert(last(*socket)["session"]=="");
  step(32000,false); assert(!fourMultiplayer.poll(snapshot)); // Allow 30s reconnection window.
  step(42000,false); assert(fourMultiplayer.poll(snapshot) && snapshot.phase==FourPhase::Error);
  fourMultiplayer.close(); fourMultiplayer.start(); step(UINT32_MAX-20);
  socket->events.push_back({WStype_CONNECTED,{}}); step(UINT32_MAX-10);
  socket->callbackDelay=1; deliver(*socket,valid,UINT32_MAX); socket->callbackDelay=0;
  assert(fourMultiplayer.online && fourMultiplayer.poll(snapshot));
  step(2999); assert(fourMultiplayer.online); step(3001); assert(!fourMultiplayer.online);
  step(40001,false); assert(fourMultiplayer.poll(snapshot) && snapshot.phase==FourPhase::Error);
  std::cout<<"PASS: Four protocol bounds, commands, revisions, rematch, reconnect, leave, epochs and clock rollover\n";
}
