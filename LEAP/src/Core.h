#pragma once
#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
namespace leap {
inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
  return uint32_t(now - since) >= duration;
}
inline bool safePath(const std::string &s) {
  if (s.empty() || s.size() > 180 || s.front() == '/' || s.back() == '/')
    return false;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == '/') {
      auto part = s.substr(start, i - start);
      if (part.empty() || part == "." || part == "..")
        return false;
      start = i + 1;
    } else if (static_cast<unsigned char>(s[i]) < 32 || s[i] == '\\' || s[i] == '?' ||
               s[i] == '#' || s[i] == '%')
      return false;
  }
  return true;
}
inline bool identifier(const std::string &s) {
  if (s.empty() || s.size() > 100)
    return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_'))
      return false;
  return true;
}
inline bool digestValid(const std::string &s) {
  if (s.size() != 64)
    return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  return true;
}
// Gregorian civil date to Unix time (Howard Hinnant's days_from_civil algorithm).
inline int64_t utcTimestamp(int year, unsigned month, unsigned day, int hour, int minute,
                            int second) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = unsigned(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = int64_t(era) * 146097 + doe - 719468;
  return days * 86400 + hour * 3600 + minute * 60 + second;
}
struct SemVersion {
  std::string parts[4];
  bool stable = false, valid = false;
  explicit SemVersion(std::string text) {
    if (text.empty() || text.size() > 40)
      return;
    auto beta = text.find("-beta.");
    stable = beta == std::string::npos;
    if (!stable) {
      parts[3] = text.substr(beta + 6);
      text = text.substr(0, beta);
    } else
      parts[3] = "0";
    for (int i = 0; i < 3; i++) {
      auto dot = text.find('.');
      if (i < 2 && dot == std::string::npos)
        return;
      if (i == 2 && dot != std::string::npos)
        return;
      parts[i] = text.substr(0, dot);
      if (i < 2)
        text = text.substr(dot + 1);
    }
    for (auto &p : parts) {
      if (p.empty() || (p.size() > 1 && p[0] == '0'))
        return;
      for (char c : p)
        if (c < '0' || c > '9')
          return;
    }
    valid = true;
  }
};
inline int compareVersion(const SemVersion &a, const SemVersion &b) {
  for (int i = 0; i < 4; i++) {
    if (i == 3 && a.stable != b.stable)
      return a.stable ? 1 : -1;
    if (a.parts[i].size() != b.parts[i].size())
      return a.parts[i].size() > b.parts[i].size() ? 1 : -1;
    if (a.parts[i] != b.parts[i])
      return a.parts[i] > b.parts[i] ? 1 : -1;
  }
  return 0;
}
inline bool meetsVersion(const std::string &installed, const std::string &minimum) {
  SemVersion a(installed), b(minimum);
  return a.valid && b.valid && compareVersion(a, b) >= 0;
}
struct Debouncer {
  bool raw = false, stable = false;
  uint32_t changed = 0, pressed = 0, repeated = 0;
  bool longSent = false;
  // 1 press, 2 repeat (directions only), 3 long press; millis wrap safe.
  int poll(bool down, uint32_t now, bool repeat) {
    if (raw != down) {
      raw = down;
      changed = now;
    }
    if (stable != raw && elapsed(now, changed, 25)) {
      stable = raw;
      if (stable) {
        pressed = repeated = now;
        longSent = false;
        return 1;
      }
    }
    if (stable && !longSent && elapsed(now, pressed, 900)) {
      longSent = true;
      return 3;
    }
    if (stable && repeat && elapsed(now, pressed, 400) && elapsed(now, repeated, 150)) {
      repeated = now;
      return 2;
    }
    return 0;
  }
};
#pragma pack(push, 1)
struct ChatPacket {
  uint8_t magic[4] = {'L', 'E', 'A', 'P'};
  uint8_t protocol = 1;
  uint32_t boot = 0, sequence = 0, templates = 0;
  uint8_t sender[6] = {};
  char message[37] = {};
  char name[33] = {};
};
#pragma pack(pop)
static_assert(sizeof(ChatPacket) <= 250, "ESP-NOW v1 payload limit");
inline bool validPacket(const ChatPacket &p, size_t n) {
  return n == sizeof(p) && !memcmp(p.magic, "LEAP", 4) && p.protocol == 1 && p.templates > 0 &&
         p.message[36] == 0 && p.name[32] == 0 && strlen(p.message) == 36;
}
struct SeenMessages {
  struct Entry {
    uint8_t sender[6];
    uint32_t boot, seq;
  };
  Entry entries[32]{};
  uint8_t head = 0;
  bool accept(const ChatPacket &p) {
    for (const auto &e : entries)
      if (!memcmp(e.sender, p.sender, 6) && e.boot == p.boot && e.seq == p.sequence)
        return false;
    auto &e = entries[head++ % 32];
    memcpy(e.sender, p.sender, 6);
    e.boot = p.boot;
    e.seq = p.sequence;
    return true;
  }
};
} // namespace leap
