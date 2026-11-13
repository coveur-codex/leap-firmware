#include "RadioStream.h"
#include "Protocol.h"
#include <cassert>
#include <cstdio>
using namespace leap;
std::string decode(const std::string &wire, bool chunked, uint32_t icy) {
  RadioBody body(chunked, icy);
  std::string output;
  for (unsigned char byte : wire) {
    int kind = body.feed(byte);
    assert(kind >= 0);
    if (kind == 1) output += char(byte);
    if (kind == 2) break;
  }
  return output;
}
int main() {
  assert(radioUrl("https://example.org:8443/live?x=1"));
  for (auto url : {"", "ftp://example.org/a", "https://", "http:///a", "https://u:p@example.org/a", "https://a/\r\nX:b", "https://a/x#fragment", "https://a/\\x"}) assert(!radioUrl(url));
  assert(radioRedirect("http://a/dir/live?token=1", "https://b/live") == "https://b/live");
  assert(radioRedirect("https://a/dir/live", "/mp3") == "https://a/mp3");
  assert(radioRedirect("https://a/dir/live", "mp3") == "https://a/dir/mp3");
  assert(radioRedirect("https://a/live", "//b/mp3") == "https://b/mp3");
  assert(radioRedirect("https://a/live?x=1", "?x=2") == "https://a/live?x=2");
  assert(decode("abc", false, 0) == "abc");
  std::string icy = std::string("abc") + char(1) + std::string(16, 'X') + "def" + char(0) + "ghi";
  assert(decode(icy, false, 3) == "abcdefghi");
  // ICY markers and metadata cross arbitrary HTTP chunk boundaries.
  std::string wire;
  for (char byte : icy) { wire += "1;ext=x\r\n"; wire += byte; wire += "\r\n"; }
  wire += "0\r\n\r\n";
  assert(decode(wire, true, 3) == "abcdefghi");
  assert(decode("3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n", true, 0) == "abcdef");
  for (auto invalid : {"z\r\n", "\r\n", "100000000\r\n", "1\n", "1\rX", "1\r\naX"}) {
    RadioBody body(true, 0); bool failed = false;
    for (uint8_t byte : std::string(invalid)) if (body.feed(byte) < 0) failed = true;
    assert(failed);
  }
  JsonDocument config;
  deserializeJson(config, R"({"deviceId":"test","configVersion":1,"age":8,"communicationEnabled":true,"pages":[{"id":"radio","order":2,"enabled":true}],"radioStations":[{"id":1,"name":"Radio","url":"https://a/live"}]})");
  assert(deviceConfig(config.as<JsonVariantConst>(), "test", 1));
  config["radioStations"][0]["url"] = "https://a/\r\nHeader:x";
  assert(!deviceConfig(config.as<JsonVariantConst>(), "test", 1));
  config["radioStations"][0]["url"] = "https://a/live";
  auto duplicate = config["radioStations"].add<JsonObject>();
  duplicate["id"] = 1; duplicate["name"] = "Other"; duplicate["url"] = "http://b/live";
  assert(!deviceConfig(config.as<JsonVariantConst>(), "test", 1));
  config.remove("radioStations"); // Compatibility with older home servers.
  assert(deviceConfig(config.as<JsonVariantConst>(), "test", 1));
  puts("PASS: MP3 URL/config bounds, redirects, HTTP chunks, ICY metadata and invalid framing");
}
