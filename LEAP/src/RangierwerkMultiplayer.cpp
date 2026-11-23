#include "RangierwerkMultiplayer.h"
#include "Communication.h"
#include "NetworkProfiles.h"
#include "Transport.h"
#include "WebSocketEndpoint.h"
#include <WiFi.h>

namespace leap {
RangierwerkMultiplayer rangierwerkMultiplayer;
namespace {
struct Command {
  uint32_t epoch, connection, card, a, b, at;
  RangierAction type;
};
RelaySocket socket;
uint32_t workerEpoch = UINT32_MAX, sequence = 0, card = 0;
uint32_t lastDrive = 0, lastPing = 0, lastFrame = 0, retryAt = 0, attemptAt = 0;
int sentDirection = 0;
bool started = false, hasRetry = false, fatal = false;
String server;
RangierFrame decoded;

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

bool RangierwerkMultiplayer::begin() {
  incoming = xQueueCreate(1, sizeof(RangierFrame));
  outgoing = xQueueCreate(8, sizeof(Command));
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
        sequence = card = 0;
        sentDirection = 0;
        lastFrame = lastPing = millis();
        JsonDocument frame(&jsonRam);
        frame["type"] = "join";
        frame["protocolVersion"] = 1;
        if (!send(frame))
          socket.disconnect();
      } else if (type == WStype_TEXT) {
        JsonDocument frame(&jsonRam);
        if (length > 4096 || deserializeJson(frame, static_cast<const uint8_t *>(payload), length) ||
            !decodeRangierFrame(frame, decoded)) {
          socket.disconnect();
          return;
        }
        lastFrame = millis();
        decoded.epoch = workerEpoch;
        if (decoded.phase == RangierPhase::Playing) {
          if (card != decoded.card) {
            direction = 0;
            sentDirection = 0;
            ++connection;
          }
          card = decoded.card;
          online = true;
        } else {
          online = false;
          fatal = decoded.phase == RangierPhase::Error;
          if (decoded.phase == RangierPhase::Full) {
            hasRetry = true;
            retryAt = lastFrame;
          }
          socket.disconnect();
          started = false;
        }
        xQueueOverwrite(incoming, &decoded);
      } else if (type == WStype_BIN || type == WStype_FRAGMENT_TEXT_START || type == WStype_FRAGMENT_BIN_START) {
        socket.disconnect();
      }
    });
    ready = xTaskCreatePinnedToCore([](void *p) { static_cast<RangierwerkMultiplayer *>(p)->run(); },
                                  "leap-yard", 8192, this, 1, nullptr, 0) == pdPASS;
  }
  if (!ready) {
    if (incoming) vQueueDelete(incoming);
    if (outgoing) vQueueDelete(outgoing);
    incoming = outgoing = nullptr;
  }
  return ready;
}

bool RangierwerkMultiplayer::start() {
  if (!ready && !begin())
    return false;
  online = false;
  direction = 0;
  ++epoch;
  enabled = true;
  return true;
}

void RangierwerkMultiplayer::close() {
  if (enabled.exchange(false)) {
    direction = 0;
    online = false;
    ++epoch;
  }
}

void RangierwerkMultiplayer::action(RangierAction type, uint32_t atCard, uint32_t a, uint32_t b) {
  if (enabled && online) {
    Command command{epoch.load(), connection.load(), atCard, a, b, millis(), type};
    xQueueSend(outgoing, &command, 0);
  }
}

bool RangierwerkMultiplayer::poll(RangierFrame &out) {
  if (!enabled || !incoming)
    return false;
  if (xQueueReceive(incoming, &out, 0) != pdTRUE)
    return false;
  if (out.epoch != epoch.load()) {
    out = RangierFrame{};
    return false;
  }
  return true;
}

void RangierwerkMultiplayer::service() {
  uint32_t now = millis();
  const uint32_t generation = epoch.load();
  if (workerEpoch != generation) {
    if (started && socket.isConnected())
      sendType("leave");
    socket.disconnect();
    started = hasRetry = fatal = false;
    card = 0;
    workerEpoch = generation;
    attemptAt = now;
  }
  if (!enabled || fatal)
    return;
  String endpoint = networkProfiles.endpoint();
  if (server != endpoint) {
    server = endpoint;
    socket.disconnect();
    started = hasRetry = false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (started) socket.disconnect();
    started = false;
  } else if (!hasRetry || elapsed(now, retryAt, 2500)) {
    if (!started) {
      WebSocketEndpoint ep;
      if (webSocketEndpoint(server.c_str(), ep) && (!ep.tls || strlen(deviceSettings.tlsCa))) {
        String path = String(ep.path.c_str()) + "/api/v1/devices/" + Transport::encode(deviceSettings.deviceId) +
                      "/rangierwerk/ws";
        socket.setReconnectInterval(1000);
        if (ep.tls)
          socket.beginSslWithCA(ep.host.c_str(), ep.port, path.c_str(), deviceSettings.tlsCa, "");
        else
          socket.begin(ep.host.c_str(), ep.port, path.c_str(), "");
        socket.enableHeartbeat(1000, 2000, 1);
        started = true;
        hasRetry = false;
      }
    }
    if (started)
      socket.loop();
    now = millis();
    if (started && socket.isConnected() && elapsed(now, lastFrame, 3500))
      socket.disconnect();
    if (online && generation == epoch.load()) {
      const int drive = direction.load();
      if (drive != sentDirection || (drive && elapsed(now, lastDrive, 120))) {
        JsonDocument frame(&jsonRam);
        frame["type"] = "drive";
        frame["seq"] = ++sequence;
        frame["card"] = card;
        frame["direction"] = drive;
        if (!send(frame))
          socket.disconnect();
        lastDrive = now;
        sentDirection = drive;
      }
      if (online && elapsed(now, lastPing, 1000)) {
        if (!sendType("ping"))
          socket.disconnect();
        lastPing = now;
      }
      Command command;
      while (xQueueReceive(outgoing, &command, 0) == pdTRUE) {
        if (!online || command.epoch != generation || command.connection != connection.load() ||
            command.card != card || elapsed(now, command.at, 600))
          continue;
        JsonDocument frame(&jsonRam);
        frame["seq"] = ++sequence;
        frame["card"] = card;
        if (command.type == RangierAction::Turnout) {
          frame["type"] = "turnout";
          frame["index"] = command.a;
        } else if (command.type == RangierAction::Split) {
          frame["type"] = "split";
          frame["a"] = command.a;
          frame["b"] = command.b;
        } else {
          frame["type"] = "recover";
        }
        if (!send(frame))
          socket.disconnect();
      }
    }
  }
  if (!online && !card && !hasRetry && elapsed(now, attemptAt, 15000)) {
    if (started) socket.disconnect();
    started = false;
    fatal = true;
    decoded = RangierFrame{};
    decoded.epoch = generation;
    decoded.phase = RangierPhase::Error;
    xQueueOverwrite(incoming, &decoded);
  }
}

void RangierwerkMultiplayer::run() {
  for (;;) {
    service();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
} // namespace leap
