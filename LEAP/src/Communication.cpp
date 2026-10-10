#include "Communication.h"
#include "Assets.h"
#include "Core.h"
#include "ChatSymbols.h"
#include "Transport.h"
#include "WebSocketEndpoint.h"
#include <WiFi.h>
namespace leap {
Communication communication;
bool Communication::begin() {
  incoming = xQueueCreate(16, sizeof(RelayDelivery));
  outgoing = xQueueCreate(8, sizeof(RelayRequest));
  socket.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    socketEvent(type, payload, length);
  });
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
  return true;
}
bool Communication::write(JsonDocument &frame) {
  std::string encoded;
  serializeJson(frame, encoded);
  String raw(encoded.c_str());
  return socket.sendTXT(raw);
}
bool Communication::subscribe() {
  JsonDocument frame(&jsonRam);
  frame["type"] = "sync";
  if (initialized) frame["since"] = cursor;
  return write(frame);
}
void Communication::socketEvent(WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_DISCONNECTED) {
    online = false;
    requestSent = false; // Keep the same eventId until acknowledged.
    if (pending && enabled && workerEpoch == epoch.load()) sendStatus = RelaySendStatus::Retrying;
    return;
  }
  if (!enabled || workerEpoch != epoch.load()) return;
  if (type == WStype_CONNECTED) {
    reconnectInterval = 2000;
    if (!subscribe()) socket.disconnect();
  } else if (type == WStype_TEXT) {
    JsonDocument frame(&jsonRam);
    if (length > 16384 || deserializeJson(frame, static_cast<const uint8_t *>(payload), length)) {
      socket.disconnect();
      return;
    }
    if (frame["type"] == "messages") {
      // Never acknowledge a malformed page or a batch that cannot fit. A
      // reconnect requests the unchanged cursor after the UI drains its queue.
      if (!deliver(frame, workerEpoch) || !subscribe()) socket.disconnect();
      else online = true;
    } else if (frame["type"] == "ack") {
      if (pending && frame["ok"] == true && frame["eventId"] == request.eventId) {
        pending = requestSent = false;
        sendStatus = RelaySendStatus::Sent;
      }
    } else if (frame["type"] == "error") {
      if (pending && frame["eventId"] == request.eventId) {
        int status = frame["status"] | 500;
        if (status >= 400 && status < 500 && status != 408 && status != 429) {
          pending = requestSent = false;
          sendStatus = RelaySendStatus::Rejected;
        } else socket.disconnect();
      }
    } else if (frame["type"] != "pong") socket.disconnect();
  } else if (type == WStype_BIN || type == WStype_FRAGMENT_TEXT_START ||
             type == WStype_FRAGMENT_BIN_START) socket.disconnect();
}
void Communication::service(uint32_t now, bool connected) {
  uint32_t generation = epoch.load();
  if (workerEpoch != generation) {
    socketStarted = false;
    socket.disconnect();
    workerEpoch = generation;
    initialized = false;
    cursor = 0;
    pending = requestSent = false;
    reconnectInterval = 2000;
    lastSocketFailure = 0;
  }
  if (!enabled || !connected) {
    if (socketStarted) socket.disconnect();
    socketStarted = false;
    online = false;
    return;
  }
  if (!socketStarted) {
    WebSocketEndpoint endpoint;
    if (!webSocketEndpoint(deviceSettings.server, endpoint) ||
        (endpoint.tls && !strlen(deviceSettings.tlsCa))) return;
    String path = String(endpoint.path.c_str()) + "/api/v1/devices/" +
        Transport::encode(deviceSettings.deviceId) + "/communication/ws";
    socket.setReconnectInterval(reconnectInterval);
    if (endpoint.tls)
      socket.beginSslWithCA(endpoint.host.c_str(), endpoint.port, path.c_str(), deviceSettings.tlsCa, "");
    else socket.begin(endpoint.host.c_str(), endpoint.port, path.c_str(), "");
    socket.enableHeartbeat(30000, 10000, 2);
    socketStarted = true;
  }
  socket.loop();
  uint32_t failure = socket.failureStamp();
  if (failure != lastSocketFailure && !socket.isConnected()) {
    socket.setReconnectInterval(reconnectInterval);
    reconnectInterval = std::min(reconnectInterval * 2, uint32_t(30000));
  }
  lastSocketFailure = failure;
  if (!enabled || epoch.load() != generation || !socket.isConnected()) return;
  if (!pending) {
    while (xQueueReceive(outgoing, &request, 0) == pdTRUE) {
      if (request.epoch == generation) { pending = true; requestSent = false; break; }
    }
  }
  if (pending && !requestSent) {
    JsonDocument frame(&jsonRam);
    frame["type"] = "send";
    frame["eventId"] = request.eventId;
    frame["templateId"] = request.templateId;
    requestSent = write(frame);
    lastAttempt = now;
    if (!requestSent) socket.disconnect();
  } else if (pending && elapsed(now, lastAttempt, 10000)) {
    socket.disconnect(); // A lost acknowledgement retries the same eventId.
  }
}
void Communication::run() {
  for (;;) {
    service(millis(), WiFi.status() == WL_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(socketStarted ? 50 : 250));
  }
}
} // namespace leap
