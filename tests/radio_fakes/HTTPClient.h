#pragma once
#include "NetworkClientSecure.h"
#include <map>
#include <stdexcept>
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
constexpr int HTTPC_ERROR_READ_TIMEOUT = -11, HTTPC_ERROR_TOO_LESS_RAM = -8;
struct RadioResponse {
  int code;
  std::map<std::string, String> headers;
  std::function<void()> action;
};
inline std::deque<RadioResponse> radioResponses;
inline std::vector<std::string> radioRequestedUrls;
inline int32_t radioConnectTimeout = 0;
inline uint16_t radioHeaderTimeout = 0;
inline std::string radioUserAgent;
inline unsigned radioHttpClosed = 0;
class HTTPClient {
  String url;
  std::map<std::string, String> headers;
public:
  void setConnectTimeout(int32_t timeout) { radioConnectTimeout = timeout; }
  void setTimeout(uint16_t timeout) { radioHeaderTimeout = timeout; }
  void useHTTP10(bool) {}
  void setReuse(bool) {}
  void setFollowRedirects(int) {}
  void collectHeaders(const char **, unsigned) {}
  void addHeader(const char *, const char *) {}
  void setUserAgent(const String &agent) { radioUserAgent = agent; }
  bool begin(NetworkClient &, const String &value) { url = value; return true; }
  int GET() {
    radioRequestedUrls.push_back(url);
    if (radioResponses.empty()) throw std::runtime_error("unexpected request");
    auto response = radioResponses.front(); radioResponses.pop_front(); headers = response.headers;
    radioNow += 2500; // An ordinary response can take longer than the old budget.
    if (response.action) response.action();
    if (radioHeaderTimeout < 2500) return HTTPC_ERROR_READ_TIMEOUT;
    return response.code;
  }
  String header(const char *key) { return headers[key]; }
  void end() { ++radioHttpClosed; }
  static String errorToString(int) { return "transport failure"; }
};
