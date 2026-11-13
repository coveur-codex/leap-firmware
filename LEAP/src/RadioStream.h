#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <algorithm>
namespace leap {
inline bool radioUrl(const std::string &url) {
  size_t start = url.compare(0, 7, "http://") == 0 ? 7 :
                 url.compare(0, 8, "https://") == 0 ? 8 : 0;
  if (!start || url.size() > 1000 || start == url.size()) return false;
  auto end = url.find_first_of("/?#", start);
  auto host = url.substr(start, end - start);
  if (host.empty() || host.find('@') != std::string::npos || url.find('#') != std::string::npos) return false;
  for (unsigned char c : url) if (c <= 32 || c == 127 || c == '\\') return false;
  return true;
}
// Log only the scheme/authority: paths and query strings can contain stream tokens.
inline std::string radioEndpoint(const std::string &url) {
  auto start = url.find("://");
  if (start == std::string::npos) return "invalid-url";
  return url.substr(0, url.find_first_of("/?#", start + 3));
}
inline std::string radioHost(const std::string &url) {
  auto endpoint = radioEndpoint(url);
  auto start = endpoint.find("://");
  if (start == std::string::npos) return {};
  auto host = endpoint.substr(start + 3);
  if (host.empty()) return {};
  if (host[0] == '[') return host.substr(1, host.find(']') - 1);
  return host.substr(0, host.find(':'));
}
inline std::string radioRedirect(const std::string &base, const std::string &target) {
  if (target.empty()) return {};
  if (target.find("://") != std::string::npos) return radioUrl(target) ? target : "";
  auto scheme = base.find("://");
  if (scheme == std::string::npos) return {};
  if (target.compare(0, 2, "//") == 0) return base.substr(0, scheme + 1) + target;
  auto slash = base.find('/', scheme + 3);
  auto origin = base.substr(0, base.find_first_of("/?", scheme + 3));
  auto path = slash == std::string::npos ? "/" : base.substr(slash);
  path = path.substr(0, path.find('?'));
  auto result = origin + (target[0] == '/' ? target : target[0] == '?' ? path + target : path.substr(0, path.rfind('/') + 1) + target);
  return radioUrl(result) ? result : "";
}
// Incremental HTTP chunk framing and ICY stripping. Never waits for a byte.
class RadioBody {
  bool chunked;
  uint32_t interval, audioLeft, metaLeft = 0, chunkLeft = 0, chunkSize = 0;
  uint8_t framing = 0, digits = 0;
  bool extension = false;
  size_t headerLength = 0;
public:
  RadioBody(bool chunks, uint32_t icy) : chunked(chunks), interval(icy), audioLeft(icy) {}
  // -1 malformed, 0 framing/metadata, 1 MP3 byte, 2 end of chunked body.
  int feed(uint8_t value) {
    if (chunked) {
      if (framing == 4) return 2;
      if (framing == 1) { if (value != '\n') return -1; framing = chunkSize ? 2 : 4; chunkLeft = chunkSize; return framing == 4 ? 2 : 0; }
      if (framing == 3) { if (value != '\r') return -1; framing = 5; return 0; }
      if (framing == 5) { if (value != '\n') return -1; framing = 0; chunkSize = digits = headerLength = 0; extension = false; return 0; }
      if (framing == 0) {
        if (++headerLength > 128) return -1;
        if (value == '\r') { if (!digits) return -1; framing = 1; return 0; }
        if (value == ';') { if (!digits) return -1; extension = true; return 0; }
        if (extension) return value >= 32 ? 0 : -1;
        int digit = value >= '0' && value <= '9' ? value - '0' :
                    value >= 'a' && value <= 'f' ? value - 'a' + 10 :
                    value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
        if (digit < 0 || ++digits > 8 || chunkSize > (UINT32_MAX - unsigned(digit)) / 16) return -1;
        chunkSize = chunkSize * 16 + unsigned(digit); return 0;
      }
      if (!--chunkLeft) framing = 3;
    }
    if (metaLeft) { --metaLeft; return 0; }
    if (interval && !audioLeft) { metaLeft = uint32_t(value) * 16; audioLeft = interval; return 0; }
    if (interval) --audioLeft;
    return 1;
  }
};
} // namespace leap
