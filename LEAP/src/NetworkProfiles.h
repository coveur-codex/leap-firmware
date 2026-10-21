#pragma once
#include "Config.h"
#include "WebSocketEndpoint.h"
#include <mutex>
#include <atomic>
#include <string>
#include <cctype>
namespace leap {
struct WifiProfile {
  char ssid[33]{}, password[65]{};
  bool valid() const {
    if (!memchr(ssid, 0, sizeof(ssid)) || !memchr(password, 0, sizeof(password))) return false;
    for (const char *s = ssid; *s; ++s) if (uint8_t(*s) < 32) return false;
    size_t n = strlen(password);
    if (!n) return true;
    if (n < 8 || n > 64) return false;
    for (const char *s = password; *s; ++s)
      if (uint8_t(*s) < 32 || (n == 64 && !std::isxdigit(uint8_t(*s)))) return false;
    return true;
  }
};
inline bool networkUrl(const char *url) {
  std::string s(url);
  size_t prefix = s.rfind("http://", 0) == 0 ? 7 : s.rfind("https://", 0) == 0 ? 8 : 0;
  if (!prefix || s.size() <= prefix || s.size() > 256) return false;
  for (size_t i = prefix; i < s.size(); ++i)
    if (uint8_t(s[i]) <= 32 || uint8_t(s[i]) >= 127 || strchr("@?#\\", s[i])) return false;
  WebSocketEndpoint parsed;
  return webSocketEndpoint(url, parsed);
}
struct NetworkRecord {
  uint32_t schema = 1, revision = 0;
  char server[257]{}, fallback[257]{};
  WifiProfile bootstrap{}, local[4]{}, remote[4]{};
  uint32_t checksum = 0;
  uint32_t hash() const {
    uint32_t value = 2166136261u;
    auto *bytes = reinterpret_cast<const uint8_t *>(this);
    for (size_t i=0; i<offsetof(NetworkRecord, checksum); ++i) value = (value ^ bytes[i])*16777619u;
    return value;
  }
  bool valid() const {
    if (schema != 1 || checksum != hash() || !memchr(server,0,sizeof(server)) ||
        !memchr(fallback,0,sizeof(fallback)) || !networkUrl(server) || !networkUrl(fallback)) return false;
    if (!bootstrap.valid()) return false;
    for (const auto &p : local) if (!p.valid()) return false;
    for (const auto &p : remote) if (!p.valid()) return false;
    return true;
  }
};
// Separate namespace/record: old OTA images can still load schema-1 device settings.
class NetworkProfiles {
  mutable std::mutex mutex;
  NetworkRecord record;
  bool ready = false, useFallback = false;
  bool commit(NetworkRecord &next) {
    next.checksum = next.hash();
    if (!next.valid()) return false;
    Preferences prefs;
    if (!prefs.begin("leap-network", false)) return false;
    bool ok = prefs.putBytes("profiles", &next, sizeof(next)) == sizeof(next);
    NetworkRecord verify;
    ok = ok && prefs.getBytes("profiles", &verify, sizeof(verify)) == sizeof(verify) &&
         verify.valid() && memcmp(&next, &verify, sizeof(next)) == 0;
    prefs.end();
    if (ok) record = next;
    return ok;
  }
public:
  std::atomic<uint32_t> wifiGeneration{0};
  bool begin() {
    std::lock_guard<std::mutex> lock(mutex);
    Preferences prefs;
    if (!prefs.begin("leap-network", true)) return false;
    size_t size = prefs.getBytesLength("profiles");
    NetworkRecord loaded;
    bool ok = size == sizeof(loaded) && prefs.getBytes("profiles", &loaded, size) == size && loaded.valid();
    prefs.end();
    if (size && !ok) return false;
    if (!size) {
      strcpy(loaded.server, "http://ndk.tstromi.de:8080");
      strncpy(loaded.fallback, deviceSettings.server, sizeof(loaded.fallback)-1);
      strncpy(loaded.bootstrap.ssid, deviceSettings.ssid, 32);
      strncpy(loaded.bootstrap.password, deviceSettings.password, 64);
      if (!commit(loaded)) return false;
    } else record = loaded;
    ready = true;
    return true;
  }
  String endpoint() const {
    std::lock_guard<std::mutex> lock(mutex);
    return ready ? (useFallback ? record.fallback : record.server) : deviceSettings.server;
  }
  void nextEndpoint() {
    std::lock_guard<std::mutex> lock(mutex);
    if (ready) useFallback = !useFallback;
  }
  NetworkRecord snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    return record;
  }
  // Local entries win duplicate SSIDs; server sync never overwrites local input.
  bool profile(size_t index, WifiProfile &out) const {
    std::lock_guard<std::mutex> lock(mutex);
    size_t n=0;
    for (const auto &p : record.local) if (p.ssid[0]) {
      if (n++ == index) { out=p; return true; }
    }
    for (const auto &p : record.remote) if (p.ssid[0]) {
      bool duplicate=false;
      for (const auto &l : record.local) if (!strcmp(l.ssid,p.ssid)) duplicate=true;
      if (!duplicate && n++ == index) { out=p; return true; }
    }
    if (record.bootstrap.ssid[0]) {
      bool duplicate=false;
      for (const auto &p : record.local) if (!strcmp(p.ssid,record.bootstrap.ssid)) duplicate=true;
      for (const auto &p : record.remote) if (!strcmp(p.ssid,record.bootstrap.ssid)) duplicate=true;
      if (!duplicate && n == index) { out=record.bootstrap; return true; }
    }
    return false;
  }
  bool saveLocal(const WifiProfile &p) {
    if (!p.ssid[0] || !p.valid()) return false;
    std::lock_guard<std::mutex> lock(mutex);
    NetworkRecord next=record;
    int found=-1;
    for (int i=0;i<4;++i) if (!strcmp(next.local[i].ssid,p.ssid)) found=i;
    if (found < 0) for (int i=0;i<4;++i) if (!next.local[i].ssid[0]) { found=i; break; }
    if (found < 0) return false;
    for (int i=found;i>0;--i) next.local[i]=next.local[i-1];
    next.local[0]=p;
    if (!commit(next)) return false;
    ++wifiGeneration;
    return true;
  }
  bool removeLocal(int index) {
    if (index<0 || index>3) return false;
    std::lock_guard<std::mutex> lock(mutex);
    NetworkRecord next=record;
    for (int i=index;i<3;++i) next.local[i]=next.local[i+1];
    next.local[3]={};
    if (!commit(next)) return false;
    ++wifiGeneration;
    return true;
  }
  bool applyRemote(uint32_t revision, const char *url, const WifiProfile (&profiles)[4]) {
    std::lock_guard<std::mutex> lock(mutex);
    if (revision == record.revision) return true;
    if (!networkUrl(url)) return false;
    NetworkRecord next=record;
    next.revision=revision;
    strncpy(next.server,url,sizeof(next.server)-1);
    memcpy(next.remote,profiles,sizeof(next.remote));
    bool wifiChanged=memcmp(record.remote,next.remote,sizeof(next.remote)) != 0;
    if (!commit(next)) return false;
    useFallback=false;
    if (wifiChanged) ++wifiGeneration;
    return true;
  }
};
inline NetworkProfiles networkProfiles;
} // namespace leap
