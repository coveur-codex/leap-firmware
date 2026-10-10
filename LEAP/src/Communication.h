#pragma once
#include "Storage.h"
#include "RelayInbox.h"
#include <atomic>
#include <WebSocketsClient.h>
namespace leap {
// The pinned library does not emit DISCONNECTED for TCP connect failures.
// Observe its failure timestamp so those failures get the same bounded backoff.
class RelaySocket : public WebSocketsClient {
public:
  uint32_t failureStamp() const { return _lastConnectionFail; }
};
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
  uint32_t workerEpoch = UINT32_MAX, lastAttempt = 0;
  uint32_t reconnectInterval = 2000;
  RelaySocket socket;
  uint32_t lastSocketFailure = 0;
  bool socketStarted = false, requestSent = false;
  uint64_t cursor = 0;
  bool initialized = false, pending = false;
  RelayRequest request;
  bool deliver(JsonDocument &response, uint32_t generation);
  void run();
  bool write(JsonDocument &frame);
  bool subscribe();
  void socketEvent(WStype_t type, uint8_t *payload, size_t length);
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
  // One bounded step; the socket and all cursor/retry state belong to the worker.
  void service(uint32_t now, bool connected);
};
extern Communication communication;
} // namespace leap
