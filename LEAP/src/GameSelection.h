#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <string>
#include <vector>
namespace leap {
inline bool builtInGame(const std::string &id) {
  for (const char *known : {"hot_potato", "simon_motion", "tilt_maze", "kitchen",
                            "snake", "connect_four", "tamagotchi", "crab_journey", "dragon_run",
                            "math_lab", "fall_blocks", "rangierwerk", "minesweeper", "breakout", "pfuetzenpost", "dice"})
    if (id == known)
      return true;
  return false;
}
inline bool unsyncedGames(JsonVariantConst config) { return config.isNull(); }
// Once provisioned by a Homeserver, only explicitly granted games are shown.
inline std::vector<std::string> enabledGames(JsonVariantConst config, bool offlineFallback = false) {
  std::vector<std::string> result;
  if (offlineFallback && unsyncedGames(config))
    return {"rangierwerk"}; // A device that has not synced can still play offline.
  for (JsonObjectConst game : config["games"].as<JsonArrayConst>()) {
    std::string id = game["id"] | "";
    if (game["enabled"] == true && builtInGame(id) && id != "math_lab" &&
        std::find(result.begin(), result.end(), id) == result.end())
      result.push_back(id);
  }
  return result;
}
} // namespace leap
