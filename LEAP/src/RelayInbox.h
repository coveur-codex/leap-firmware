#pragma once
#include <cstdint>
#include <cstddef>
namespace leap {
struct RelayLine {
  uint64_t id = 0;
  char name[401]{}, text[481]{}, symbol[65]{};
  bool mine = false;
};
struct RelayDelivery {
  RelayLine line;
  uint32_t epoch = 0;
  bool initial = false, reset = false;
};
// Main/UI-task state; the network task only passes fixed-size queue records.
class RelayInbox {
  uint64_t last = 0;
public:
  RelayLine history[8];
  size_t count = 0;
  bool unread = false;
  uint32_t revision = 0;
  void clear() { count = 0; last = 0; unread = false; ++revision; }
  bool accept(const RelayDelivery &delivery) {
    if (delivery.reset) { clear(); return false; }
    const auto &line = delivery.line;
    if (!line.id || line.id <= last) return false;
    last = line.id;
    if (count == 8) {
      for (int i = 1; i < 8; ++i) history[i-1] = history[i];
      --count;
    }
    history[count++] = line;
    ++revision;
    bool notify = !delivery.initial && !line.mine;
    if (notify) unread = true;
    return notify;
  }
  void markRead() { if (unread) { unread = false; ++revision; } }
};
} // namespace leap
