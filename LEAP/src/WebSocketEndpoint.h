#pragma once
#include <string>
#include <cstdint>
namespace leap {
struct WebSocketEndpoint {
  std::string host, path;
  uint16_t port = 0;
  bool tls = false;
};
inline bool webSocketEndpoint(const char *base, WebSocketEndpoint &out) {
  std::string url(base);
  out = WebSocketEndpoint{};
  size_t start = url.compare(0, 8, "https://") == 0 ? 8 :
                 url.compare(0, 7, "http://") == 0 ? 7 : 0;
  if (!start || url.find_first_of("\r\n\\?#@ ") != std::string::npos) return false;
  out.tls = start == 8;
  out.port = out.tls ? 443 : 80;
  size_t slash = url.find('/', start);
  std::string authority = url.substr(start, slash == std::string::npos ? slash : slash-start);
  if (authority.empty()) return false;
  size_t colon = authority.find(':');
  if (authority[0] == '[') {
    size_t end = authority.find(']');
    if (end == std::string::npos || end == 1) return false;
    out.host = authority.substr(1, end-1);
    colon = end+1 == authority.size() ? std::string::npos : end+1;
    if (colon != std::string::npos && authority[colon] != ':') return false;
  } else out.host = authority.substr(0, colon);
  if (out.host.empty()) return false;
  if (colon != std::string::npos) {
    std::string port = authority.substr(colon+1);
    if (port.empty() || port.size() > 5) return false;
    unsigned number = 0;
    for (char c : port) { if (c < '0' || c > '9') return false; number = number*10 + c-'0'; }
    if (!number || number > 65535) return false;
    out.port = number;
  }
  if (slash != std::string::npos) out.path = url.substr(slash);
  while (!out.path.empty() && out.path.back() == '/') out.path.pop_back();
  return out.path.find("..") == std::string::npos;
}
} // namespace leap
