#include "Communication.h"
#include "Assets.h"
#include "Core.h"
#include "ChatSymbols.h"
#include "Transport.h"
#include <WiFi.h>
namespace leap {
Communication communication;
bool Communication::begin() {
  incoming = xQueueCreate(16, sizeof(RelayDelivery));
  outgoing = xQueueCreate(8, sizeof(RelayRequest));
  ready = incoming && outgoing;
  if (!ready) return false;
  ready = xTaskCreatePinnedToCore([](void *p) { static_cast<Communication *>(p)->run(); },
      "leap-chat", 8192, this, 1, nullptr, 0) == pdPASS;
  return ready;
}
void Communication::configure(JsonDocument &state) {
  bool allowed = state["config"]["communicationEnabled"] == true;
  if (enabled.exchange(allowed) != allowed) {
    ++epoch;
    online = false;
    clear();
    sendStatus = RelaySendStatus::Idle;
  }
  templates.clear();
  int version = state["assets"]["communication-messages"] | 0;
  if (allowed && version) {
    JsonDocument definition(&jsonRam);
    if (assets.definition("communication-messages", version, definition)) {
      String path = assets.resolve("communication-messages", version,
          definition["messagesFile"] | "messages.json");
      if (!storage.readJson(path, templates) || templates["schemaVersion"] != 1) templates.clear();
    }
  }
}
bool Communication::send(size_t index) {
  if (index >= messages().size()) { sendStatus = RelaySendStatus::LocalBlocked; return false; }
  return enqueue(messages()[index]["id"] | "");
}
bool Communication::sendIcon(size_t index) {
  if (index >= ChatIconCount) { sendStatus = RelaySendStatus::LocalBlocked; return false; }
  return enqueue(ChatIcons[index].id);
}
bool Communication::enqueue(const char *id) {
  if (!ready || !enabled || !id[0] || strlen(id) >= sizeof(request.templateId) ||
      !elapsed(millis(), lastSend, 1000)) {
    sendStatus = RelaySendStatus::LocalBlocked;
    return false;
  }
  RelayRequest message;
  message.epoch = epoch.load();
  strlcpy(message.templateId, id, sizeof(message.templateId));
  // 128-bit random ID remains identical on every retry, independent of uptime.
  snprintf(message.eventId, sizeof(message.eventId), "%08lx%08lx%08lx%08lx",
      (unsigned long)esp_random(), (unsigned long)esp_random(),
      (unsigned long)esp_random(), (unsigned long)esp_random());
  sendStatus = RelaySendStatus::Queued;
  if (xQueueSend(outgoing, &message, 0) != pdTRUE) {
    sendStatus = RelaySendStatus::LocalBlocked;
    return false;
  }
  lastSend = millis();
  return true;
}
bool Communication::poll() {
  bool notify = false;
  RelayDelivery delivery;
  while (incoming && xQueueReceive(incoming, &delivery, 0) == pdTRUE)
    if (enabled && delivery.epoch == epoch.load()) notify = accept(delivery) || notify;
  return notify;
}
static bool validLine(JsonVariantConst value, uint64_t previous) {
  if (!value["id"].is<uint64_t>() || value["id"].as<uint64_t>() <= previous) return false;
  for (auto field : {"senderId", "name", "text", "symbol"})
    if (!value[field].is<const char *>()) return false;
  return strlen(value["senderId"]) <= 80 && strlen(value["name"]) <= 400 &&
      strlen(value["text"]) <= 480 && strlen(value["symbol"]) <= 64;
}
bool Communication::deliver(JsonDocument &response, uint32_t generation) {
  bool reset = response["reset"] == true;
  bool initial = !initialized || reset;
  auto rows = response["messages"].as<JsonArrayConst>();
  if (response["schemaVersion"] != 1 || !response["messages"].is<JsonArrayConst>() || rows.size() > 8 ||
      !response["cursor"].is<uint64_t>() || !response["more"].is<bool>() || !response["reset"].is<bool>()) return false;
  uint64_t next = response["cursor"], previous = initial ? 0 : cursor;
  if ((!initial && next < cursor) || (response["more"] == true && !rows.size())) return false;
  for (JsonVariantConst row : rows) {
    if (!validLine(row, previous)) return false;
    previous = row["id"];
  }
  if (previous > next || (rows.size() && previous != next)) return false;
  // One producer, one consuming UI: reserve the entire batch before advancing
  // the server cursor. A stalled UI causes backpressure, never silent loss.
  if (!enabled || epoch.load() != generation ||
      uxQueueSpacesAvailable(incoming) < rows.size() + unsigned(initial)) return false;
  RelayDelivery delivery;
  delivery.epoch = generation;
  delivery.initial = initial;
  if (initial) {
    delivery.reset = true;
    if (xQueueSend(incoming, &delivery, 0) != pdTRUE) return false;
    delivery.reset = false;
  }
  for (JsonVariantConst row : rows) {
    delivery.line = RelayLine{};
    delivery.line.id = row["id"];
    delivery.line.mine = !strcmp(row["senderId"], deviceSettings.deviceId);
    strlcpy(delivery.line.name, row["name"], sizeof(delivery.line.name));
    strlcpy(delivery.line.text, row["text"], sizeof(delivery.line.text));
    strlcpy(delivery.line.symbol, row["symbol"], sizeof(delivery.line.symbol));
    if (xQueueSend(incoming, &delivery, 0) != pdTRUE) return false;
  }
  cursor = next;
  initialized = true;
  pollInterval = response["more"] == true ? 0 : 2000;
  return true;
}
void Communication::service(uint32_t now, bool connected,
    const std::function<bool(const String &, JsonDocument &, JsonDocument *)> &json,
    const std::function<int()> &httpStatus) {
  uint32_t generation = epoch.load();
  if (workerEpoch != generation) {
    workerEpoch = generation;
    initialized = false;
    cursor = 0;
    pollInterval = retryInterval = 0;
    pending = false;
  }
  if (!enabled || !connected) { online = false; return; }
  String base = "/api/v1/devices/" + Transport::encode(deviceSettings.deviceId) + "/communication/messages";
  if (!pending) {
    while (outgoing && xQueueReceive(outgoing, &request, 0) == pdTRUE) {
      if (request.epoch == generation) { pending = true; retryInterval = 0; break; }
    }
  }
  if (pending && elapsed(now, lastAttempt, retryInterval)) {
    JsonDocument body(&jsonRam), response(&jsonRam);
    body["eventId"] = request.eventId;
    body["templateId"] = request.templateId;
    bool sent = json(base, response, &body) && response["ok"] == true && response["eventId"] == request.eventId;
    if (epoch.load() != generation || !enabled) return;
    if (sent) {
      pending = false;
      sendStatus = RelaySendStatus::Sent;
      pollInterval = 0; // Fetch the confirmed message without skipping intervening sends.
    } else if (httpStatus() >= 400 && httpStatus() < 500 && httpStatus() != 429 && httpStatus() != 408) {
      pending = false;
      sendStatus = RelaySendStatus::Rejected;
    } else {
      sendStatus = RelaySendStatus::Retrying;
      retryInterval = retryInterval ? std::min(retryInterval * 2, uint32_t(30000)) : 2000;
    }
    lastAttempt = now;
  }
  if (elapsed(now, lastPoll, pollInterval)) {
    JsonDocument response(&jsonRam);
    char query[48]{};
    if (initialized) snprintf(query, sizeof(query), "?since=%llu", (unsigned long long)cursor);
    bool fetched = json(base + query, response, nullptr);
    if (epoch.load() != generation || !enabled) return;
    online = fetched;
    if (!fetched || !deliver(response, generation)) {
      online = false;
      pollInterval = 5000;
    }
    lastPoll = now;
  }
}
void Communication::run() {
  Transport transport;
  for (;;) {
    service(millis(), WiFi.status() == WL_CONNECTED,
        [&](const String &path, JsonDocument &response, JsonDocument *body) {
          return transport.json(path, response, body);
        }, [&] { return transport.status; });
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
} // namespace leap
