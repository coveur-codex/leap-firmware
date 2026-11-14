#pragma once
#include <Arduino.h>
inline int radioTlsError = 0;
inline unsigned radioHandshakeTimeout = 0;
inline bool radioBundleUsed = false;
inline unsigned fakeTlsClientsCreated = 0, fakeTlsClientsAlive = 0;
class NetworkClient {
public:
  std::deque<uint8_t> body;
  virtual ~NetworkClient() = default;
  virtual int available() { return int(body.size()); }
  virtual int read(uint8_t *out, size_t size) { size=std::min(size,body.size()); for (size_t i=0;i<size;++i) { out[i]=body.front(); body.pop_front(); } return int(size); }
  virtual bool connected() { return !body.empty(); }
};
class NetworkClientSecure : public NetworkClient {
public:
  NetworkClientSecure() { ++fakeTlsClientsCreated; ++fakeTlsClientsAlive; }
  ~NetworkClientSecure() override { --fakeTlsClientsAlive; }
  void setCACert(const char *) {}
  void setCACertBundle(const uint8_t *, size_t) { radioBundleUsed = true; }
  void setHandshakeTimeout(unsigned seconds) { radioHandshakeTimeout = seconds; }
  int lastError(char *out, size_t size) { strlcpy(out, "certificate verify failed", size); return radioTlsError; }
};
