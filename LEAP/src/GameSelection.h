#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <string>
#include <vector>
namespace leap {
inline bool builtInGame(const std::string &id) {
  for (const char *known : {"hot_potato", "simon_motion", "tilt_maze", "kitchen",
                            "snake", "connect_four", "tamagotchi", "crab_journey", "dragon_run"})
    if (id == known)
      return true;
  return false;
}
// One list drives both rendering and launching. Missing/false entries grant no access.
inline std::vector<std::string> enabledGames(JsonVariantConst config) {
  std::vector<std::string> result;
  for (JsonObjectConst game : config["games"].as<JsonArrayConst>()) {
    std::string id = game["id"] | "";
    if (game["enabled"] == true && builtInGame(id) &&
        std::find(result.begin(), result.end(), id) == result.end())
      result.push_back(id);
  }
  return result;
}
} // namespace leap
