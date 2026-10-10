#pragma once
#include "Storage.h"
#include "RelayInbox.h"
#include <atomic>
#include <functional>
namespace leap {
struct RelayRequest {
  char eventId[65]{}, templateId[37]{};
  uint32_t epoch = 0;
};
enum class RelaySendStatus { Idle, Queued, Sent, Retrying, Rejected, LocalBlocked };
class Communication : public RelayInbox {
  QueueHandle_t incoming = nullptr, outgoing = nullptr;
  JsonDocument templates{&jsonRam};
  std::atomic<uint32_t> epoch{0};
  bool ready = false;
  uint32_t lastSend = 0;
  // Worker-task-owned state.
  uint32_t workerEpoch = UINT32_MAX, lastPoll = 0, lastAttempt = 0;
  uint32_t pollInterval = 0, retryInterval = 0;
  uint64_t cursor = 0;
  bool initialized = false, pending = false;
  RelayRequest request;
  bool deliver(JsonDocument &response, uint32_t generation);
  void run();
  bool enqueue(const char *id);
public:
  std::atomic<bool> enabled{false}, online{false};
  std::atomic<RelaySendStatus> sendStatus{RelaySendStatus::Idle};
  bool begin();
  void configure(JsonDocument &state);
  bool poll(); // Consume messages and return true for a new incoming notification.
  bool send(size_t index);
  bool sendIcon(size_t index);
  JsonArrayConst messages() const { return templates["messages"].as<JsonArrayConst>(); }
  // One bounded worker step; injection makes the actual retry/queue path testable.
  void service(uint32_t now, bool connected,
      const std::function<bool(const String &, JsonDocument &, JsonDocument *)> &json,
      const std::function<int()> &httpStatus);
};
extern Communication communication;
} // namespace leap
