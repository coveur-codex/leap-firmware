#include "GameSelection.h"
#include <cassert>
#include <iostream>
using namespace leap;
int main() {
  JsonDocument config;
  assert(enabledGames(config).empty());
  assert(enabledGames(config, true) == std::vector<std::string>{"rangierwerk"});
  assert(!deserializeJson(config, R"({"games":[
    {"id":"kitchen","enabled":false}, {"id":"unknown","enabled":true},
    {"id":"snake","enabled":true}, {"id":"snake","enabled":true},
    {"id":"tamagotchi","enabled":"true"}, {"id":"connect_four"}]})"));
  assert(enabledGames(config) == std::vector<std::string>{"snake"});
  config["games"][0]["enabled"] = true;
  assert((enabledGames(config) == std::vector<std::string>{"kitchen", "snake"}));
  config["games"][0]["enabled"] = false;
  config["games"][2]["enabled"] = false;
  config["games"][3]["enabled"] = false;
  assert(enabledGames(config).empty());
  assert(enabledGames(config, true).empty());
  auto games = config["games"].to<JsonArray>();
  for (const char *id : {"tamagotchi", "snake", "hot_potato", "simon_motion", "tilt_maze",
                         "connect_four", "kitchen", "crab_journey", "dragon_run", "math_lab",
                         "fall_blocks", "rangierwerk", "minesweeper", "breakout", "pfuetzenpost", "dice"}) {
    auto game = games.add<JsonObject>();
    game["id"] = id;
    game["enabled"] = true;
  }
  assert(enabledGames(config).size() == 15 && enabledGames(config).back() == "dice");
  games[games.size() - 1]["enabled"] = false;
  auto available = enabledGames(config);
  assert(available.back() == "pfuetzenpost" && available.size() == 14);
  games[games.size() - 1]["enabled"] = true;
  // Snapshot round-trip is the same representation used for offline startup.
  std::string saved;
  serializeJson(config, saved);
  JsonDocument offline;
  assert(!deserializeJson(offline, saved));
  assert(enabledGames(offline) == enabledGames(config));
  std::cout << "PASS: game selection, unknown IDs, disabled kitchen, duplicates and offline snapshot\n";
}
