#include "SnakeMultiplayer.h"
#include "Communication.h"
#include "NetworkProfiles.h"
#include "Transport.h"
#include "WebSocketEndpoint.h"
#include <WiFi.h>
namespace leap {
SnakeMultiplayer snakeMultiplayer;
struct SnakeTurn { uint32_t epoch, tick; uint8_t direction; };
// Socket and mutable protocol state belong exclusively to the network worker.
static RelaySocket socket;
static uint32_t workerEpoch = UINT32_MAX, seq=0, lastFrame=0, lastPing=0, beganAt=0;
static uint32_t revision=0, tick=0;
static bool started=false, fatal=false, received=false;
static String server, session;
static SnakeSnapshot decoded;
static bool writeFrame(JsonDocument &doc) {
  std::string encoded; serializeJson(doc,encoded);
  String raw(encoded.c_str()); return socket.sendTXT(raw);
}
bool SnakeMultiplayer::begin() {
  incoming=xQueueCreate(1,sizeof(SnakeSnapshot));
  outgoing=xQueueCreate(8,sizeof(SnakeTurn));
  if (!incoming || !outgoing) return false;
  socket.onEvent([this](WStype_t type,uint8_t *payload,size_t length) {
    if (type==WStype_DISCONNECTED) { online=false; return; }
    if (!enabled || workerEpoch!=epoch.load()) return;
    if (type==WStype_CONNECTED) {
      JsonDocument f(&jsonRam); f["type"]="join"; f["session"]=session; f["protocolVersion"]=2;
      if (!writeFrame(f)) socket.disconnect();
    } else if (type==WStype_TEXT) {
      JsonDocument f(&jsonRam);
      if (length>16384 || deserializeJson(f,static_cast<const uint8_t*>(payload),length)) {
        socket.disconnect(); return;
      }
      if (f["type"]=="error") {
        decoded=SnakeSnapshot{}; decoded.phase=SnakePhase::Error;
        strlcpy(decoded.error,"Partie beendet. Bitte neu starten.",sizeof(decoded.error));
        fatal=true;
      } else {
        if (!decodeSnake(f,decoded) || (session.length() && session!=decoded.session)) {
          socket.disconnect(); return;
        }
        if (received && (decoded.revision<revision || decoded.tick<tick)) return;
        session=decoded.session; revision=decoded.revision; tick=decoded.tick;
        for (unsigned i=0;i<decoded.count;++i)
          if (!strcmp(decoded.players[i].id,deviceSettings.deviceId))
            seq=std::max(seq,decoded.players[i].seq);
        received=true; online=true;
      }
      lastFrame=millis(); decoded.epoch=workerEpoch;
      xQueueOverwrite(incoming,&decoded);
    } else if (type==WStype_BIN || type==WStype_FRAGMENT_TEXT_START || type==WStype_FRAGMENT_BIN_START)
      socket.disconnect();
  });
  ready=xTaskCreatePinnedToCore([](void *p){static_cast<SnakeMultiplayer*>(p)->run();},
      "leap-snake",12288,this,1,nullptr,0)==pdPASS;
  return ready;
}
bool SnakeMultiplayer::start() {
  if (!ready) return false;
  enabled=false; ++epoch; enabled=true; online=false;
  return true;
}
void SnakeMultiplayer::close() { enabled=false; ++epoch; online=false; }
void SnakeMultiplayer::turn(uint8_t direction,uint32_t atTick) {
  if (!enabled || !online || direction>3) return;
  SnakeTurn t{epoch.load(),atTick,direction}; xQueueSend(outgoing,&t,0);
}
bool SnakeMultiplayer::poll(SnakeSnapshot &out) {
  static SnakeSnapshot delivery;
  if (!incoming || xQueueReceive(incoming,&delivery,0)!=pdTRUE ||
      !enabled || delivery.epoch!=epoch.load()) return false;
  out=delivery;
  return true;
}
void SnakeMultiplayer::service(uint32_t now,bool connected) {
  uint32_t generation=epoch.load();
  if (workerEpoch!=generation) {
    socket.disconnect(); started=false; workerEpoch=generation;
    session=""; seq=revision=tick=0; received=fatal=false; beganAt=lastFrame=now;
  }
  String endpoint=networkProfiles.endpoint();
  if (endpoint!=server) { server=endpoint; socket.disconnect(); started=false; }
  if (!enabled || !connected || fatal) {
    if (started) socket.disconnect();
    started=false; online=false;
  } else {
    if (!started) {
      WebSocketEndpoint ep;
      if (webSocketEndpoint(server.c_str(),ep) && (!ep.tls || strlen(deviceSettings.tlsCa))) {
        String path=String(ep.path.c_str())+"/api/v1/devices/"+Transport::encode(deviceSettings.deviceId)+"/snake/ws";
        socket.setReconnectInterval(1000);
        if (ep.tls) socket.beginSslWithCA(ep.host.c_str(),ep.port,path.c_str(),deviceSettings.tlsCa,"");
        else socket.begin(ep.host.c_str(),ep.port,path.c_str(),"");
        socket.enableHeartbeat(1000,2000,1); started=true;
      }
    }
    if (started) socket.loop();
    // Callbacks timestamp received frames with millis(). Read the clock again
    // so a fresh frame cannot appear newer than now and underflow the timeout.
    now=millis();
    if (socket.isConnected() && now-lastPing>=1000) {
      JsonDocument f(&jsonRam); f["type"]="ping"; writeFrame(f); lastPing=now;
    }
    SnakeTurn turn;
    while (xQueueReceive(outgoing,&turn,0)==pdTRUE) {
      if (turn.epoch!=generation || !online || turn.tick!=tick) continue;
      JsonDocument f(&jsonRam); f["type"]="input"; f["seq"]=++seq;
      f["tick"]=turn.tick; f["direction"]=turn.direction;
      if (!writeFrame(f)) socket.disconnect();
    }
    if (online && now-lastFrame>3000) { online=false; socket.disconnect(); }
  }
  if (enabled && !fatal && now-(received?lastFrame:beganAt)>15000) {
    decoded=SnakeSnapshot{}; decoded.epoch=generation; decoded.phase=SnakePhase::Error;
    strlcpy(decoded.error,"Homeserver nicht erreichbar. Neustart möglich.",sizeof(decoded.error));
    xQueueOverwrite(incoming,&decoded); fatal=true;
  }
}
void SnakeMultiplayer::run() {
  for (;;) { service(millis(),WiFi.status()==WL_CONNECTED); vTaskDelay(pdMS_TO_TICKS(20)); }
}
} // namespace leap
