#pragma once
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include <algorithm>
namespace leap {
inline JsonVariantConst learningSettings(JsonVariantConst config) {
  for (JsonObjectConst page : config["pages"].as<JsonArrayConst>())
    if (page["id"] == "learning") return page["settings"];
  return {};
}
inline bool hasLearningPage(JsonVariantConst config) {
  for (JsonObjectConst page : config["pages"].as<JsonArrayConst>())
    if (page["id"] == "learning") return true;
  return false;
}
inline bool legacyMathLab(JsonVariantConst config) {
  if (hasLearningPage(config)) return false;
  bool gamesEnabled = false;
  for (JsonObjectConst page : config["pages"].as<JsonArrayConst>())
    if (page["id"] == "games" && page["enabled"] == true) gamesEnabled = true;
  for (JsonObjectConst game : config["games"].as<JsonArrayConst>())
    if (gamesEnabled && game["id"] == "math_lab" && game["enabled"] == true) return true;
  return false;
}
inline std::vector<std::string> enabledLearningApps(JsonVariantConst config) {
  if (legacyMathLab(config)) return {"math_lab"};
  std::vector<std::string> result;
  for (JsonObjectConst page : config["pages"].as<JsonArrayConst>()) {
    if (page["id"] != "learning" || page["enabled"] != true) continue;
    for (JsonObjectConst app : page["settings"]["applications"].as<JsonArrayConst>()) {
      std::string id = app["id"] | "";
      if (app["enabled"] == true && (id == "math_lab" || id == "syllable_trainer") &&
          std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
    }
  }
  return result;
}
} // namespace leap
