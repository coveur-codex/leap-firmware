#include "SnakeMultiplayer.h"
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
static JsonDocument state(unsigned tick=0,unsigned revision=1) {
  JsonDocument f;
  f["type"]="state"; f["schemaVersion"]=1; f["session"]=std::string(32,'a');
  f["columns"]=40; f["rows"]=13; f["stepMs"]=450;
  f["tick"]=tick; f["revision"]=revision; f["remainingMs"]=120000;
  f["phase"]="playing"; f["score"]=0; f["target"]=20;
  auto p=f["players"].to<JsonArray>().add<JsonObject>();
  p["id"]="local"; p["avatar"]="avatar-dragon"; p["accentColor"]="#ff9933";
  p["connected"]=true; p["protected"]=false; p["seq"]=0; p["respawnMs"]=0; p["direction"]=3;
  auto c=p["body"].to<JsonArray>().add<JsonArray>(); c.add(5); c.add(5);
  c=f["food"].to<JsonArray>().add<JsonArray>(); c.add(8); c.add(8);
  return f;
}
static void step(unsigned now,bool wifi=true) { fakeNow=now; snakeMultiplayer.service(now,wifi); }
static void deliver(WebSocketsClient &socket,JsonDocument &f,unsigned now) {
  std::string raw; serializeJson(f,raw);
  socket.events.push_back({WStype_TEXT,raw}); step(now);
}
static JsonDocument last(WebSocketsClient &socket) {
  JsonDocument f; assert(!socket.sent.empty()); assert(!deserializeJson(f,socket.sent.back())); return f;
}
int main() {
  SnakeSnapshot snapshot;
  auto valid=state(); assert(decodeSnake(valid,snapshot));
  auto invalid=state(); invalid["players"][0]["body"][0][0]=40;
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["food"][0][0]=5; invalid["food"][0][1]=5;
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["players"].as<JsonArray>().add(valid["players"][0]);
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["remainingMs"]=120001; assert(!decodeSnake(invalid,snapshot));
  strcpy(deviceSettings.deviceId,"local");
  strcpy(deviceSettings.server,"http://homeserver:8080");
  assert(snakeMultiplayer.begin() && snakeMultiplayer.start());
  step(100);
  // The existing relay transport has one instance of its own.
  WebSocketsClient *socket=nullptr;
  for (auto s:WebSocketsClient::instances) if (s->url.find("/snake/ws")!=std::string::npos) socket=s;
  assert(socket && socket->interval==1000);
  socket->events.push_back({WStype_CONNECTED,{}}); step(200);
  assert(last(*socket)["type"]=="join" && last(*socket)["session"]=="");
  deliver(*socket,valid,300);
  assert(snakeMultiplayer.online && snakeMultiplayer.poll(snapshot));
  assert(snapshot.phase==SnakePhase::Playing && snapshot.count==1);
  snakeMultiplayer.turn(0,0); step(400);
  assert(last(*socket)["type"]=="input" && last(*socket)["seq"]==1 && last(*socket)["tick"]==0);
  auto newer=state(2,4); deliver(*socket,newer,450);
  auto older=state(1,3); deliver(*socket,older,460);
  assert(snakeMultiplayer.poll(snapshot) && snapshot.tick==2);
  assert(!snakeMultiplayer.poll(snapshot));
  // Queue retains the newest complete state under a stalled UI.
  auto latest=state(3,5); deliver(*socket,newer,470); deliver(*socket,latest,480);
  assert(snakeMultiplayer.poll(snapshot) && snapshot.tick==3);
  step(500,false); assert(!snakeMultiplayer.online);
  step(600); socket->events.push_back({WStype_CONNECTED,{}}); step(700);
  assert(last(*socket)["type"]=="join" && last(*socket)["session"]==std::string(32,'a'));
  deliver(*socket,latest,800); snakeMultiplayer.close(); snakeMultiplayer.start();
  assert(!snakeMultiplayer.poll(snapshot)); // Previous visit cannot replace UI state.
  step(900); socket->events.push_back({WStype_CONNECTED,{}}); step(950);
  assert(last(*socket)["session"]=="");
  step(17000,false); assert(snakeMultiplayer.poll(snapshot) && snapshot.phase==SnakePhase::Error);
  std::cout << "PASS: bounded Snake decoder, input sequence, latest snapshot, reconnect identity, stale visit and timeout\n";
}
