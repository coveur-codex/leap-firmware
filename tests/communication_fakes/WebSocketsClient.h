#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>
enum WStype_t { WStype_DISCONNECTED, WStype_CONNECTED, WStype_TEXT, WStype_BIN,
 WStype_FRAGMENT_TEXT_START, WStype_FRAGMENT_BIN_START };
class WebSocketsClient {
  std::function<void(WStype_t,uint8_t*,size_t)> callback;
protected:
  uint32_t _lastConnectionFail = 0;
public:
  static inline std::vector<WebSocketsClient*> instances;
  bool connected = false, started = false, failConnect = false;
  unsigned interval=0, heartbeat=0;
  std::string host, url, ca;
  uint16_t port=0;
  std::vector<std::string> sent;
  std::vector<std::pair<WStype_t,std::string>> events;
  WebSocketsClient() { instances.push_back(this); }
  void onEvent(std::function<void(WStype_t,uint8_t*,size_t)> cb) { callback=cb; }
  void begin(const char *h, uint16_t p, const char *u, const char *) { host=h; port=p; url=u; started=true; _lastConnectionFail=0; }
  void beginSslWithCA(const char *h, uint16_t p, const char *u, const char *cert, const char *protocol) { ca=cert; begin(h,p,u,protocol); }
  void setReconnectInterval(unsigned n) { interval=n; }
  void enableHeartbeat(unsigned n, unsigned, unsigned) { heartbeat=n; }
  bool isConnected() { return connected; }
  bool sendTXT(String &s) { if (!connected) return false; sent.push_back(s); return true; }
  void disconnect() { if (connected) { connected=false; _lastConnectionFail=fakeNow; callback(WStype_DISCONNECTED,nullptr,0); } }
  void loop() {
    if (failConnect) { _lastConnectionFail=fakeNow; return; }
    if (events.empty()) return;
    auto event=events.front(); events.erase(events.begin());
    if (event.first==WStype_CONNECTED) { connected=true; _lastConnectionFail=0; }
    if (event.first==WStype_DISCONNECTED) { connected=false; _lastConnectionFail=fakeNow; }
    callback(event.first, reinterpret_cast<uint8_t*>(event.second.data()), event.second.size());
  }
};
