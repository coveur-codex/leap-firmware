#pragma once
#include "WiFi.h"
#include <cerrno>
#include <stdexcept>
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
struct TransportReply {
  int status;
  std::string body;
  int size = -2;
  int tlsError = 0;
  int socketError = 0;
};
inline std::deque<TransportReply> transportReplies;
inline std::string transportMethod, transportBody;
inline unsigned transportClosed = 0;
class FakeHttpStream {
  std::string data;
  size_t position = 0;
public:
  void set(std::string value) { data = std::move(value); position = 0; }
  size_t available() const { return data.size() - position; }
  int read(uint8_t *out, size_t count) {
    count = std::min(count, available()); memcpy(out, data.data() + position, count); position += count; return int(count);
  }
};
class HTTPClient {
  int size = -1;
  FakeHttpStream stream;
  int request(const char *method) {
    if (transportReplies.empty()) throw std::runtime_error("unexpected request");
    auto reply = transportReplies.front(); transportReplies.pop_front();
    transportMethod = method; size = reply.size == -2 ? int(reply.body.size()) : reply.size;
    stream.set(reply.body); radioTlsError = reply.tlsError; errno = reply.socketError; radioNow += 13;
    return reply.status;
  }
public:
  void setConnectTimeout(int32_t) {}
  void setTimeout(uint32_t) {}
  void setFollowRedirects(int) {}
  void useHTTP10(bool) {}
  bool begin(WiFiClient &, const String &) { return true; }
  void addHeader(const char *, const char *) {}
  int GET() { return request("GET"); }
  int POST(const String &body) { transportBody = body; return request("POST"); }
  int getSize() { return size; }
  FakeHttpStream *getStreamPtr() { return &stream; }
  bool connected() { return stream.available() != 0; }
  void end() { ++transportClosed; radioTlsError = 0; }
  static String errorToString(int) { return "connection refused"; }
};
