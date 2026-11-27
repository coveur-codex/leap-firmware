#include "PondMultiplayer.h"
#include "AccentColor.h"
#include <algorithm>
#include <cstring>

namespace leap {
namespace {
void copyText(char *dest, size_t size, const char *source) {
  size_t count = std::min(size - 1, std::strlen(source));
  while (count && (uint8_t(source[count]) & 0xc0) == 0x80)
    --count;
  std::memcpy(dest, source, count);
  dest[count] = 0;
}

bool bounded(JsonVariantConst value, unsigned max) {
  return value.is<unsigned>() && value.as<unsigned>() <= max;
}

bool lane(JsonVariantConst value) {
  return bounded(value, PondLanes - 1) || (value.is<unsigned>() && value.as<unsigned>() == PondNone);
}
} // namespace

bool decodePondFrame(JsonVariantConst doc, PondFrame &out) {
  PondFrame frame;
  const char *type = doc["type"] | "";
  if (!std::strcmp(type, "error")) {
    frame.phase = PondPhase::Error;
    copyText(frame.reason, sizeof(frame.reason), doc["code"] | "");
    out = frame;
    return true;
  }
  if (std::strcmp(type, "state") || doc["schemaVersion"] != 1)
    return false;
  const char *session = doc["session"] | "";
  if (std::strlen(session) != 32 || std::strspn(session, "0123456789abcdef") != 32)
    return false;
  copyText(frame.session, sizeof(frame.session), session);
  const char *phase = doc["phase"] | "";
  if (!std::strcmp(phase, "lobby")) frame.phase = PondPhase::Lobby;
  else if (!std::strcmp(phase, "ready")) frame.phase = PondPhase::Ready;
  else if (!std::strcmp(phase, "rolling")) frame.phase = PondPhase::Rolling;
  else if (!std::strcmp(phase, "settling")) frame.phase = PondPhase::Settling;
  else if (!std::strcmp(phase, "paused")) frame.phase = PondPhase::Paused;
  else if (!std::strcmp(phase, "finished")) frame.phase = PondPhase::Finished;
  else return false;
  if (!bounded(doc["self"], PondLanes - 1) || !lane(doc["host"]) || !lane(doc["turn"]) ||
      !bounded(doc["winners"], (1 << PondLanes) - 1) || !bounded(doc["remainingMs"], 45000) ||
      !doc["move"].is<uint32_t>() || !doc["turnId"].is<uint32_t>() || !doc["revision"].is<uint32_t>() ||
      !doc["round"].is<uint32_t>() || !doc["ack"].is<uint32_t>())
    return false;
  frame.self = doc["self"];
  frame.host = doc["host"];
  frame.turn = doc["turn"];
  frame.winners = doc["winners"];
  frame.move = doc["move"];
  frame.turnId = doc["turnId"];
  frame.revision = doc["revision"];
  frame.round = doc["round"];
  frame.ack = doc["ack"];
  frame.remainingMs = doc["remainingMs"];
  copyText(frame.reason, sizeof(frame.reason), doc["reason"] | "");
  if (doc["positions"].size() != PondLanes || doc["previous"].size() != PondLanes ||
      doc["frogs"].size() != PondLanes || doc["dice"].size() != 2)
    return false;
  for (uint8_t i = 0; i < PondLanes; ++i) {
    auto frog = doc["frogs"][i];
    auto position = doc["positions"][i];
    auto previous = doc["previous"][i];
    if (!bounded(position, PondGoal) || !bounded(previous, position.as<unsigned>()) ||
        position.as<unsigned>() - previous.as<unsigned>() > 2 || !frog["name"].is<const char *>() ||
        !frog["color"].is<const char *>() || !frog["human"].is<bool>() || !frog["connected"].is<bool>())
      return false;
    frame.positions[i] = position;
    frame.previous[i] = previous;
    copyText(frame.frogs[i].name, sizeof(frame.frogs[i].name), frog["name"]);
    frame.frogs[i].color = accentColor(frog["color"]);
    frame.frogs[i].human = frog["human"];
    frame.frogs[i].connected = frog["connected"];
  }
  for (uint8_t i = 0; i < frame.dice.size(); ++i) {
    if (!lane(doc["dice"][i]) || (frame.move > 0 && !bounded(doc["dice"][i], PondLanes - 1)))
      return false;
    frame.dice[i] = doc["dice"][i];
  }
  out = frame;
  return true;
}
} // namespace leap
