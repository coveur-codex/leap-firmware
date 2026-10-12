#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
namespace leap {
enum class NotificationSound : uint8_t { Silent, Pling, Plong, Swish };
struct Notification {
  std::string service, title, text, symbol;
  uint16_t color = 0x06b8;
  NotificationSound sound = NotificationSound::Pling;
  unsigned count = 1;
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
    if (showing && uint32_t(now - started) >= Duration) showing = false;
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
  struct State { bool inside = false; uint32_t seen = 0; };
  std::map<std::string, State> planes;
public:
  bool observe(const std::string &id, double horizontalKm, double altitudeFeet,
               double observerMeters, uint32_t now, bool initial = false) {
    if (id.empty() || !std::isfinite(horizontalKm) || horizontalKm < 0 ||
        !std::isfinite(altitudeFeet) || !std::isfinite(observerMeters)) return false;
    if (!planes.count(id) && planes.size() >= 128) return false;
    auto &state = planes[id]; state.seen = now;
    double distance = std::hypot(horizontalKm, (altitudeFeet * 0.3048 - observerMeters) / 1000);
    if (distance > 3.5) state.inside = false;
    if (distance > 3 || state.inside) return false;
    state.inside = true;
    return !initial;
  }
  void prune(uint32_t now) {
    for (auto it = planes.begin(); it != planes.end();)
      if (uint32_t(now - it->second.seen) > 600000) it = planes.erase(it); else ++it;
  }
};
} // namespace leap
