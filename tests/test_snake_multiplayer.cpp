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
  f["columns"]=SnakeGrid::Columns; f["rows"]=SnakeGrid::Rows; f["stepMs"]=450;
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
  auto invalid=state(); invalid["players"][0]["body"][0][0]=SnakeGrid::Columns;
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["food"][0][0]=5; invalid["food"][0][1]=5;
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["players"].as<JsonArray>().add(valid["players"][0]);
  assert(!decodeSnake(invalid,snapshot));
  invalid=state(); invalid["remainingMs"]=120001; assert(!decodeSnake(invalid,snapshot));
  assert(snapshot.columns==53 && snapshot.rows==16);
  auto edge=state(); edge["players"][0]["body"][0][0]=52; edge["players"][0]["body"][0][1]=15;
  assert(decodeSnake(edge,snapshot));
  edge["players"][0]["body"][0][1]=16; assert(!decodeSnake(edge,snapshot));
  auto legacy=state(); legacy["columns"]=40; legacy["rows"]=13;
  assert(decodeSnake(legacy,snapshot) && snapshot.columns==40 && snapshot.rows==13);
  legacy["columns"]=54; assert(!decodeSnake(legacy,snapshot));
  auto full=state(); full["food"].to<JsonArray>().clear();
  auto body=full["players"][0]["body"].to<JsonArray>();
  for (unsigned i=0;i<SnakeGrid::Capacity;++i) {
    auto c=body.add<JsonArray>(); c.add(i%SnakeGrid::Columns); c.add(i/SnakeGrid::Columns);
  }
  std::string largest; serializeJson(full,largest);
  assert(largest.size()<16384 && decodeSnake(full,snapshot));
  assert(snapshot.players[0].length==SnakeGrid::Capacity);
  auto extra=body.add<JsonArray>(); extra.add(0); extra.add(0);
  assert(!decodeSnake(full,snapshot));
  strcpy(deviceSettings.deviceId,"local");
  strcpy(deviceSettings.server,"http://homeserver:8080");
  assert(snakeMultiplayer.begin() && snakeMultiplayer.start());
  step(100);
  // The existing relay transport has one instance of its own.
  WebSocketsClient *socket=nullptr;
  for (auto s:WebSocketsClient::instances) if (s->url.find("/snake/ws")!=std::string::npos) socket=s;
  assert(socket && socket->interval==1000);
  socket->events.push_back({WStype_CONNECTED,{}}); step(200);
  assert(last(*socket)["type"]=="join" && last(*socket)["session"]=="" && last(*socket)["protocolVersion"]==2);
  // Real network callbacks run later than the timestamp at service entry.
  // Even one millisecond must not wrap the unsigned inactivity subtraction.
  socket->callbackDelay=1;
  deliver(*socket,valid,300);
  socket->callbackDelay=0;
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
  // Keep receiving for longer than the 15s inactivity deadline, with callbacks
  // crossing millis() on every pass. Only real silence may trigger a timeout.
  snakeMultiplayer.close(); snakeMultiplayer.start(); step(20000);
  socket->events.push_back({WStype_CONNECTED,{}}); step(20020);
  socket->callbackDelay=7;
  for (unsigned i=0;i<20;++i) {
    auto frame=state(i,i+1); deliver(*socket,frame,20040+i*1000);
    assert(snakeMultiplayer.online && snakeMultiplayer.poll(snapshot));
    assert(snapshot.phase==SnakePhase::Playing && snapshot.tick==i);
  }
  socket->callbackDelay=0;
  step(42048); assert(!snakeMultiplayer.online); // Actual 3001ms of silence.
  step(54048,false);
  assert(snakeMultiplayer.poll(snapshot) && snapshot.phase==SnakePhase::Error);
  // The same receive-time calculation must work at the real millis() rollover.
  snakeMultiplayer.close(); snakeMultiplayer.start(); step(UINT32_MAX-20);
  socket->events.push_back({WStype_CONNECTED,{}}); step(UINT32_MAX-10);
  socket->callbackDelay=1; deliver(*socket,valid,UINT32_MAX); socket->callbackDelay=0;
  assert(snakeMultiplayer.online && snakeMultiplayer.poll(snapshot));
  assert(snapshot.phase==SnakePhase::Playing);
  step(2999); assert(snakeMultiplayer.online);
  step(3001); assert(!snakeMultiplayer.online);
  step(15001,false);
  assert(snakeMultiplayer.poll(snapshot) && snapshot.phase==SnakePhase::Error);
  std::cout << "PASS: callback timing, millis rollover, genuine inactivity and bounded Snake decoder, input sequence, latest snapshot, reconnect identity, stale visit and timeout\n";
}
