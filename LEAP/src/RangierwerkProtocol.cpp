#include "RangierwerkMultiplayer.h"
#include "AccentColor.h"

namespace leap {
namespace {
bool word(JsonVariantConst value, uint32_t upper = UINT32_MAX) {
  return value.is<uint32_t>() && value.as<uint32_t>() <= upper;
}
bool index(JsonVariantConst value, unsigned limit) {
  return word(value, limit - 1) || (word(value, rangierwerk::None) && value.as<unsigned>() == rangierwerk::None);
}
} // namespace

bool decodeRangierFrame(JsonVariantConst json, RangierFrame &out) {
  using namespace rangierwerk;
  out = RangierFrame{};
  if (json["type"] == "full" || json["type"] == "error") {
    out.phase = json["type"] == "full" ? RangierPhase::Full : RangierPhase::Error;
    return true;
  }
  if (json["type"] != "state" || json["protocolVersion"] != 1 || !word(json["card"]) ||
      !json["card"].as<uint32_t>() || !word(json["seed"]) || !word(json["difficulty"], 4) ||
      !json["difficulty"].as<unsigned>() || !word(json["remainingMs"], 600000) ||
      !word(json["switches"]) || !word(json["delivered"]) || !word(json["self"], Engines - 1) ||
      !json["summary"].is<bool>())
    return false;
  auto colors = json["colors"].as<JsonArrayConst>();
  auto vehicles = json["vehicles"].as<JsonArrayConst>();
  auto orders = json["orders"].as<JsonArrayConst>();
  if (colors.isNull() || vehicles.isNull() || orders.isNull() || colors.size() > Engines ||
      vehicles.size() > MaxVehicles || orders.size() > MaxOrders)
    return false;

  out.phase = RangierPhase::Playing;
  out.card = json["card"];
  out.seed = json["seed"];
  out.difficulty = json["difficulty"];
  out.self = json["self"];
  out.remaining = json["remainingMs"];
  out.switches = json["switches"];
  out.delivered = json["delivered"];
  out.summary = json["summary"];
  for (JsonArrayConst row : colors) {
    if (row.size() != 4 || !word(row[0], Engines - 1) || !row[1].is<const char *>() ||
        !row[2].is<const char *>() || !row[3].is<bool>())
      return false;
    const auto slot = row[0].as<unsigned>();
    const char *name = row[2].as<const char *>();
    if (std::strlen(name) >= sizeof(out.names[slot]))
      return false;
    std::strcpy(out.names[slot], name);
    out.colors[slot] = accentColor(row[1].as<const char *>());
    if (row[3].as<bool>())
      out.connected |= 1u << slot;
  }
  for (JsonArrayConst row : vehicles) {
    if (row.size() != 9 || !word(row[0], MaxVehicles - 1) || !word(row[1]) || !row[1].as<uint32_t>() ||
        !word(row[2], MaxNodes - 1) || !index(row[3], MaxNodes) || !index(row[4], CargoTypes) ||
        !word(row[5], UnloadMs) || !index(row[6], MaxVehicles) || !index(row[7], MaxVehicles) ||
        !word(row[8], 1))
      return false;
    const auto slot = row[0].as<unsigned>();
    Vehicle &v = out.vehicles[slot];
    if (v.id || ((slot < Engines) != (row[4].as<unsigned>() == None)))
      return false;
    v.id = row[1]; v.node = row[2]; v.anchor = row[3]; v.cargo = row[4]; v.unloading = row[5];
    v.links[0] = row[6]; v.links[1] = row[7]; v.anchorDirection = row[8].as<unsigned>() ? 1 : -1;
  }
  if (!out.vehicles[out.self].id)
    return false;
  unsigned n = 0;
  for (JsonArrayConst row : orders) {
    if (row.size() != 4 || !word(row[0]) || !row[0].as<uint32_t>() || !word(row[1], CargoTypes - 1) ||
        !word(row[2], 3) || !word(row[3], 2) || row[3].as<unsigned>() >= row[2].as<unsigned>())
      return false;
    auto &o = out.orders[n++];
    o.id = row[0]; o.cargo = row[1]; o.count = row[2]; o.delivered = row[3];
  }
  return true;
}
} // namespace leap
