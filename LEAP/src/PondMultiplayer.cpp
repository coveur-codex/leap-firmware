#include "PondMultiplayer.h"
#include "Communication.h"
#include "Core.h"
#include "NetworkProfiles.h"
#include "Transport.h"
#include "WebSocketEndpoint.h"
#include <WiFi.h>
#include <algorithm>

namespace leap {
PondMultiplayer pondMultiplayer;
namespace {
struct Command {
  PondAction action;
  uint32_t epoch, connection, turnId;
};
RelaySocket socket;
// Allow for the server's heartbeat detection window before its 30s reconnect grace.
constexpr uint32_t ReconnectTimeoutMs = 52000;
uint32_t workerEpoch = UINT32_MAX, sequence = 0, lastFrame = 0, lastPing = 0;
uint32_t attemptAt = 0, lastContact = 0;
bool opened = false, fatal = false, received = false;
String server, session;
PondFrame decoded;

bool send(JsonDocument &frame) {
  std::string encoded;
  serializeJson(frame, encoded);
  String raw(encoded.c_str());
  return socket.sendTXT(raw);
}
bool sendType(const char *type) {
  JsonDocument frame(&jsonRam);
  frame["type"] = type;
  return send(frame);
}
} // namespace

bool PondMultiplayer::begin() {
  incoming = xQueueCreate(1, sizeof(PondFrame));
  outgoing = xQueueCreate(4, sizeof(Command));
  if (incoming && outgoing) {
    socket.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
      if (type == WStype_DISCONNECTED) {
        online = false;
        ++connection;
        return;
      }
      if (!enabled || workerEpoch != epoch.load())
        return;
      if (type == WStype_CONNECTED) {
        lastFrame = lastPing = millis();
        JsonDocument frame(&jsonRam);
        frame["type"] = "join";
        frame["protocolVersion"] = 1;
        frame["session"] = session;
        if (!send(frame))
          socket.disconnect();
      } else if (type == WStype_TEXT) {
        JsonDocument frame(&jsonRam);
        if (length > 2048 || deserializeJson(frame, static_cast<const uint8_t *>(payload), length)) {
          socket.disconnect();
          return;
        }
        if (frame["type"] == "pong") {
          lastFrame = lastContact = millis();
          return;
        }
        PondFrame next;
        if (!decodePondFrame(frame, next)) {
          socket.disconnect();
          return;
        }
        lastFrame = millis();
        if (next.phase == PondPhase::Error) {
          fatal = true;
          socket.disconnect();
        } else {
          if ((session.length() && session != next.session) || next.revision < decoded.revision) {
            socket.disconnect();
            return;
          }
          lastContact = lastFrame;
          sequence = std::max(sequence, next.ack);
          session = next.session;
          received = true;
          online = true;
        }
        next.epoch = workerEpoch;
        decoded = next;
        xQueueOverwrite(incoming, &decoded);
      } else if (type == WStype_BIN || type == WStype_FRAGMENT_TEXT_START || type == WStype_FRAGMENT_BIN_START) {
        socket.disconnect();
      }
    });
    initialized = xTaskCreatePinnedToCore([](void *context) { static_cast<PondMultiplayer *>(context)->run(); },
                                          "leap-pond", 8192, this, 1, nullptr, 0) == pdPASS;
  }
  if (!initialized) {
    if (incoming) vQueueDelete(incoming);
    if (outgoing) vQueueDelete(outgoing);
    incoming = outgoing = nullptr;
  }
  return initialized;
}

bool PondMultiplayer::start() {
  if (!initialized && !begin())
    return false;
  online = false;
  ++epoch;
  enabled = true;
  return true;
}

void PondMultiplayer::close() {
  if (enabled.exchange(false)) {
    online = false;
    ++epoch;
  }
}

bool PondMultiplayer::action(PondAction action, uint32_t turnId) {
  Command command{action, epoch.load(), connection.load(), turnId};
  return enabled && online && xQueueSend(outgoing, &command, 0) == pdTRUE;
}

bool PondMultiplayer::poll(PondFrame &out) {
  PondFrame frame;
  if (!enabled || !incoming || xQueueReceive(incoming, &frame, 0) != pdTRUE || frame.epoch != epoch.load())
    return false;
  out = frame;
  return true;
}

void PondMultiplayer::service(uint32_t now, bool connected) {
  const uint32_t generation = epoch.load();
  if (workerEpoch != generation) {
    if (opened && socket.isConnected())
      sendType("leave");
    socket.disconnect();
    opened = fatal = received = false;
    decoded = PondFrame{};
    session = "";
    sequence = 0;
    workerEpoch = generation;
    attemptAt = lastFrame = lastContact = now;
  }
  if (!enabled || fatal)
    return;
  String endpoint = networkProfiles.endpoint();
  if (server != endpoint) {
    server = endpoint;
    socket.disconnect();
    opened = false;
  }
  if (!connected) {
    if (opened) socket.disconnect();
    opened = false;
  } else {
    if (!opened) {
      WebSocketEndpoint ep;
      if (webSocketEndpoint(server.c_str(), ep) && (!ep.tls || strlen(deviceSettings.tlsCa))) {
        String path = String(ep.path.c_str()) + "/api/v1/devices/" + Transport::encode(deviceSettings.deviceId) +
                      "/pfuetzenpost/ws";
        socket.setReconnectInterval(1000);
        if (ep.tls)
          socket.beginSslWithCA(ep.host.c_str(), ep.port, path.c_str(), deviceSettings.tlsCa, "");
        else
          socket.begin(ep.host.c_str(), ep.port, path.c_str(), "");
        socket.enableHeartbeat(5000, 3000, 1);
        opened = true;
      }
    }
    if (opened)
      socket.loop();
    now = millis();
    if (opened && socket.isConnected() && elapsed(now, lastFrame, 15000))
      socket.disconnect();
    if (online && elapsed(now, lastPing, 5000)) {
      if (!sendType("ping")) socket.disconnect();
      lastPing = now;
    }
    Command command;
    while (xQueueReceive(outgoing, &command, 0) == pdTRUE) {
      if (!online || command.epoch != generation || command.connection != connection.load() ||
          command.turnId != decoded.turnId)
        continue;
      JsonDocument frame(&jsonRam);
      frame["type"] = command.action == PondAction::Start ? "start" : command.action == PondAction::Roll ? "roll" : "stop";
      frame["seq"] = ++sequence;
      frame["turnId"] = command.turnId;
      if (!send(frame)) socket.disconnect();
    }
  }
  if (!online && elapsed(now, received ? lastContact : attemptAt, received ? ReconnectTimeoutMs : 12000)) {
    socket.disconnect();
    fatal = true;
    decoded = PondFrame{};
    decoded.epoch = generation;
    decoded.phase = PondPhase::Error;
    xQueueOverwrite(incoming, &decoded);
  }
}

void PondMultiplayer::run() {
  for (;;) {
    service(millis(), WiFi.status() == WL_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
} // namespace leap
