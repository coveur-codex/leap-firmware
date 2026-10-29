#include "FourMultiplayer.h"
#include "Communication.h"
#include "NetworkProfiles.h"
#include "Transport.h"
#include "WebSocketEndpoint.h"
#include <WiFi.h>
namespace leap {
FourMultiplayer fourMultiplayer;
struct FourCommand { uint32_t epoch, round, move; uint8_t column; bool rematch; };
// Socket and mutable protocol state belong exclusively to the network worker.
static RelaySocket socket;
static uint32_t workerEpoch = UINT32_MAX, seq=0, lastFrame=0, lastPing=0, beganAt=0;
static uint32_t revision=0, round=0, move=0;
static bool started=false, fatal=false, received=false;
static String server, session;
static FourSnapshot decoded;
static bool writeFrame(JsonDocument &doc) {
  std::string encoded; serializeJson(doc,encoded);
  String raw(encoded.c_str()); return socket.sendTXT(raw);
}
bool FourMultiplayer::begin() {
  incoming=xQueueCreate(1,sizeof(FourSnapshot));
  outgoing=xQueueCreate(8,sizeof(FourCommand));
  if (!incoming || !outgoing) return false;
  socket.onEvent([this](WStype_t type,uint8_t *payload,size_t length) {
    if (type==WStype_DISCONNECTED) { online=false; return; }
    if (!enabled || workerEpoch!=epoch.load()) return;
    if (type==WStype_CONNECTED) {
      JsonDocument f(&jsonRam); f["type"]="join"; f["session"]=session; f["protocolVersion"]=1;
      if (!writeFrame(f)) socket.disconnect();
    } else if (type==WStype_TEXT) {
      JsonDocument f(&jsonRam);
      if (length>4096 || deserializeJson(f,static_cast<const uint8_t*>(payload),length)) {
        socket.disconnect(); return;
      }
      if (f["type"]=="error") {
        decoded=FourSnapshot{}; decoded.phase=FourPhase::Error;
        strlcpy(decoded.error,"Partie beendet. Bitte neu starten.",sizeof(decoded.error));
        fatal=true;
      } else {
        if (!decodeFour(f,decoded) || (session.length() && session!=decoded.session)) {
          socket.disconnect(); return;
        }
        if (received && (decoded.revision<revision || decoded.round<round || (decoded.round==round && decoded.move<move))) return;
        session=decoded.session; revision=decoded.revision; round=decoded.round; move=decoded.move;
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
  ready=xTaskCreatePinnedToCore([](void *p){static_cast<FourMultiplayer*>(p)->run();},
      "leap-four",8192,this,1,nullptr,0)==pdPASS;
  return ready;
}
bool FourMultiplayer::start() {
  if (!ready) return false;
  enabled=false; ++epoch; enabled=true; online=false;
  return true;
}
void FourMultiplayer::close() { enabled=false; ++epoch; online=false; }
void FourMultiplayer::command(uint8_t column,uint32_t atRound,uint32_t atMove,bool rematch) {
  if (!enabled || !online || column>6) return;
  FourCommand t{epoch.load(),atRound,atMove,column,rematch}; xQueueSend(outgoing,&t,0);
}
bool FourMultiplayer::poll(FourSnapshot &out) {
  static FourSnapshot delivery;
  if (!incoming || xQueueReceive(incoming,&delivery,0)!=pdTRUE ||
      !enabled || delivery.epoch!=epoch.load()) return false;
  out=delivery;
  return true;
}
void FourMultiplayer::service(uint32_t now,bool connected) {
  uint32_t generation=epoch.load();
  if (workerEpoch!=generation) {
    if (started && socket.isConnected()) {
      JsonDocument f(&jsonRam); f["type"]="leave"; writeFrame(f);
    }
    socket.disconnect(); started=false; workerEpoch=generation;
    session=""; seq=revision=round=move=0; received=fatal=false; beganAt=lastFrame=now;
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
        String path=String(ep.path.c_str())+"/api/v1/devices/"+Transport::encode(deviceSettings.deviceId)+"/connect-four/ws";
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
    FourCommand turn;
    while (xQueueReceive(outgoing,&turn,0)==pdTRUE) {
      if (turn.epoch!=generation || !online || turn.round!=round || turn.move!=move) continue;
      JsonDocument f(&jsonRam); f["type"]=turn.rematch ? "rematch" : "input"; f["seq"]=++seq;
      f["round"]=turn.round; f["move"]=turn.move; f["column"]=turn.column;
      if (!writeFrame(f)) socket.disconnect();
    }
    if (online && now-lastFrame>3000) { online=false; socket.disconnect(); }
  }
  if (enabled && !fatal && now-(received?lastFrame:beganAt)>40000) {
    decoded=FourSnapshot{}; decoded.epoch=generation; decoded.phase=FourPhase::Error;
    strlcpy(decoded.error,"Homeserver nicht erreichbar. Neustart möglich.",sizeof(decoded.error));
    xQueueOverwrite(incoming,&decoded); fatal=true;
  }
}
void FourMultiplayer::run() {
  for (;;) { service(millis(),WiFi.status()==WL_CONNECTED); vTaskDelay(pdMS_TO_TICKS(20)); }
}
} // namespace leap
