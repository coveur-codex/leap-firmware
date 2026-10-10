#include "AccentColor.h"
#include "WebSocketEndpoint.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace leap;
  assert(accentColor("#00d7c5") == 0x06b8);
  assert(accentColor("#FF0000") == 0xf800);
  assert(accentColor("#00ff00") == 0x07e0);
  assert(accentColor("#0000ff") == 0x001f);
  for (auto bad : {"", "#123", "#gggggg", "red", "#1234567"}) assert(accentColor(bad)==0x06b8);
  WebSocketEndpoint e;
  assert(webSocketEndpoint("http://leap:8080/", e) && !e.tls && e.port==8080 && e.host=="leap" && e.path.empty());
  assert(webSocketEndpoint("https://leap/prefix/", e) && e.tls && e.port==443 && e.path=="/prefix");
  assert(webSocketEndpoint("http://[::1]:8080", e) && e.host=="::1" && e.port==8080);
  for (auto bad : {"ws://leap", "http://", "http://leap:0", "http://leap:65536", "http://leap:x", "http://user@leap", "http://leap/?q", "http://leap/../"})
    assert(!webSocketEndpoint(bad,e));
  puts("PASS: RGB565 accent defaults and validated HTTP/WSS endpoint conversion");
}
