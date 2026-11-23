#include "RangierwerkMultiplayer.h"
#include "AccentColor.h"
#include <cassert>
#include <fstream>
#include <iterator>

using namespace leap;
using namespace rangierwerk;

int main() {
  std::ifstream fixture("tests/rangierwerk-v1.json");
  std::string contents(std::istreambuf_iterator<char>(fixture), {});
  JsonDocument json;
  assert(!deserializeJson(json, contents));
  RangierFrame frame;
  assert(decodeRangierFrame(json, frame));
  assert(frame.phase == RangierPhase::Playing && frame.card == 10 && frame.self == 0);
  assert(frame.remaining == 600000 && frame.connected == 1 && !frame.summary);
  assert(frame.colors[0] == accentColor("#08d4a2") && std::string(frame.names[0]) == "Lok 0");
  Yard yard;
  assert(yard.reset(frame.seed, frame.difficulty, true) && yard.join(frame.self));
  for (Index i = 0; i < MaxVehicles; ++i) {
    assert(frame.vehicles[i].id == yard.vehicles[i].id);
    assert(frame.vehicles[i].node == yard.vehicles[i].node);
  }
  for (Index i = 0; i < MaxOrders; ++i) {
    assert(frame.orders[i].id == yard.orders[i].id);
    assert(frame.orders[i].cargo == yard.orders[i].cargo);
    assert(frame.orders[i].count == yard.orders[i].count);
  }
  json["vehicles"][0][6] = MaxVehicles;
  assert(!decodeRangierFrame(json, frame));
  json["type"] = "full";
  assert(decodeRangierFrame(json, frame) && frame.phase == RangierPhase::Full);
}
