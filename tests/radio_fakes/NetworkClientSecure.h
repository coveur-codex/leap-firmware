#pragma once
#include <Arduino.h>
inline int radioTlsError = 0;
inline unsigned radioHandshakeTimeout = 0;
inline bool radioBundleUsed = false;
class NetworkClient {
public:
  virtual ~NetworkClient() = default;
  virtual int available() { return 0; }
  virtual int read(uint8_t *, size_t) { return 0; }
  virtual bool connected() { return false; }
};
class NetworkClientSecure : public NetworkClient {
public:
  void setCACertBundle(const uint8_t *, size_t) { radioBundleUsed = true; }
  void setHandshakeTimeout(unsigned seconds) { radioHandshakeTimeout = seconds; }
  int lastError(char *out, size_t size) { strlcpy(out, "certificate verify failed", size); return radioTlsError; }
};
