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
  std::vector<uint8_t> body{};
  bool supportRange=false;
};
inline std::deque<RadioResponse> radioResponses;
inline std::vector<std::string> radioRequestedUrls;
inline int32_t radioConnectTimeout = 0;
inline uint16_t radioHeaderTimeout = 0;
inline std::string radioUserAgent;
inline unsigned radioHttpClosed = 0;
inline std::vector<std::map<std::string,String>> radioRequestHeaders;
class HTTPClient {
  String url;
  NetworkClient *client=nullptr;
  std::map<std::string,String> requestHeaders;
  std::map<std::string, String> headers;
public:
  void setConnectTimeout(int32_t timeout) { radioConnectTimeout = timeout; }
  void setTimeout(uint16_t timeout) { radioHeaderTimeout = timeout; }
  void useHTTP10(bool) {}
  void setReuse(bool) {}
  void setFollowRedirects(int) {}
  void collectHeaders(const char **, unsigned) {}
  void addHeader(const char *key, const char *value) { requestHeaders[key]=value; }
  void setUserAgent(const String &agent) { radioUserAgent = agent; }
  bool begin(NetworkClient &socket, const String &value) { client=&socket; url = value; return true; }
  int GET() {
    radioRequestedUrls.push_back(url);
    if (radioResponses.empty()) throw std::runtime_error("unexpected request");
    auto response = radioResponses.front(); radioResponses.pop_front(); headers = response.headers;
    radioNow += 2500; // An ordinary response can take longer than the old budget.
    radioRequestHeaders.push_back(requestHeaders);
    if (response.action) response.action();
    size_t offset=0;
    if (response.supportRange && requestHeaders.count("Range")) {
      offset=strtoul(requestHeaders["Range"].c_str()+6,nullptr,10);
      headers["Content-Range"]=String("bytes ")+String(offset)+"-"+String(response.body.size()-1)+"/"+String(response.body.size());
      response.code=206;
    }
    if (!response.body.empty()) {
      if (!headers.count("Content-Length")) headers["Content-Length"]=String(response.body.size()-offset);
      client->body.assign(response.body.begin()+offset,response.body.end());
    }
    if (radioHeaderTimeout < 2500) return HTTPC_ERROR_READ_TIMEOUT;
    return response.code;
  }
  String header(const char *key) { return headers[key]; }
  void end() { ++radioHttpClosed; }
  static String errorToString(int) { return "transport failure"; }
};
