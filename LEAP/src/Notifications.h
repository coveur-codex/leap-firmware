#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <set>
namespace leap {
enum class NotificationSound : uint8_t { Silent, Pling, Plong, Swish };
struct Notification {
  std::string service, title, text, symbol;
  uint16_t color = 0x06b8;
  NotificationSound sound = NotificationSound::Pling;
  unsigned count = 1;
  std::string target{};
  uint32_t duration() const { return service == "news" || service == "aircraft" ? 10000 : 5000; }
};
// UI-task owned. Services submit presentation data; the UI owns timing and acknowledgement.
class Notifications {
  std::deque<Notification> pending;
  std::map<std::string, bool> markers;
  Notification current;
  bool showing = false;
  uint32_t started = 0;
public:
  static constexpr uint32_t Duration = 5000;
  void submit(Notification message, bool bundle = false) {
    markers[message.service] = true;
    if (bundle) {
      for (auto &queued : pending) if (queued.service == message.service) {
        message.count += queued.count;
        queued = std::move(message);
        return;
      }
    }
    if (pending.size() == 12) pending.pop_front();
    pending.push_back(std::move(message));
  }
  // True only on a new presentation, so sound plays once per visible card.
  bool tick(uint32_t now) {
    if (showing && uint32_t(now - started) >= current.duration()) showing = false;
    if (showing || pending.empty()) return false;
    current = std::move(pending.front()); pending.pop_front();
    showing = true; started = now;
    return true;
  }
  const Notification *visible() const { return showing ? &current : nullptr; }
  bool unread(const std::string &service) const {
    auto found = markers.find(service); return found != markers.end() && found->second;
  }
  void markRead(const std::string &service) { markers[service] = false; }
};
inline int notificationMinutes(const char *value) {
  if (!value || std::string(value).size() != 5 || value[2] != ':') return -1;
  for (int i : {0, 1, 3, 4}) if (value[i] < '0' || value[i] > '9') return -1;
  int hours = (value[0] - '0') * 10 + value[1] - '0';
  int minutes = (value[3] - '0') * 10 + value[4] - '0';
  return hours < 24 && minutes < 60 ? hours * 60 + minutes : -1;
}
inline bool notificationQuiet(bool enabled, const char *start, const char *end, int localMinute) {
  if (!enabled) return false;
  int from = notificationMinutes(start), to = notificationMinutes(end);
  if (from < 0 || to < 0 || from == to) return false;
  if (localMinute < 0 || localMinute >= 1440) return true; // Stay quiet until the clock is known.
  return from < to ? localMinute >= from && localMinute < to : localMinute >= from || localMinute < to;
}
class NewsNotifications {
  uint64_t highest = 0;
public:
  bool observe(uint64_t id, int64_t fetched, int64_t bootEpoch) {
    bool fresh = bootEpoch > 0 && fetched > bootEpoch && id > highest;
    highest = std::max(highest, id);
    return fresh;
  }
};
// Missing/stale observations never rearm a plane. Only a measured exit does.
class AircraftProximity {
  std::set<std::string> inside;
public:
  bool observe(const std::string &id, double horizontalKm, double altitudeFeet,
               double observerMeters, uint32_t, bool initial = false) {
    if (id.empty() || !std::isfinite(horizontalKm) || horizontalKm < 0 ||
        !std::isfinite(altitudeFeet) || !std::isfinite(observerMeters)) return false;
    double distance = std::hypot(horizontalKm, (altitudeFeet * 0.3048 - observerMeters) / 1000);
    if (distance > 3.5) inside.erase(id);
    if (distance > 3 || inside.count(id) || inside.size() >= 128) return false;
    inside.insert(id);
    return !initial;
  }
};

} // namespace leap
