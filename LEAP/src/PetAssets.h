#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <string>
#include <vector>
namespace leap {
inline std::string petPathPart(std::string path, bool parent) {
  for (char &c : path)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  size_t slash = path.rfind('/');
  if (!parent) {
    std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    size_t dot = name.rfind('.');
    return name.substr(0, dot);
  }
  if (slash == std::string::npos)
    return "";
  path.resize(slash);
  slash = path.rfind('/');
  return path.substr(slash == std::string::npos ? 0 : slash + 1);
}
inline const char *petBackgroundPeriod(const std::string &path) {
  for (const char *period : {"day", "night"}) {
    std::string name = "background_" + std::string(period);
    if (petPathPart(path, false) == name || petPathPart(path, true) == name)
      return period;
  }
  return "";
}
inline bool petStateName(const std::string &name) {
  for (const char *state :
       {"idle", "happy", "sad", "hungry", "tired", "dirty", "eating", "playing", "sleeping"})
    if (name == state)
      return true;
  return false;
}
// Resolve once from the installed manifest, without changing its hashed metadata.
// Old avatar uploads often have files/animations but no tamagotchi object.
inline void petPresentation(JsonVariantConst manifest, JsonDocument &out) {
  out.clear();
  if (manifest["definition"]["tamagotchi"].is<JsonObjectConst>())
    out.set(manifest["definition"]["tamagotchi"]);
  std::vector<std::string> paths;
  for (JsonObjectConst file : manifest["files"].as<JsonArrayConst>()) {
    std::string path = file["path"] | "";
    std::string lower = path;
    for (char &c : lower)
      if (c >= 'A' && c <= 'Z')
        c += 32;
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".png")
      paths.push_back(path);
  }
  std::sort(paths.begin(), paths.end());
  JsonDocument discovered;
  for (const auto &path : paths) {
    auto state = petPathPart(path, true);
    if (petStateName(state)) {
      auto frames = discovered[state]["frames"].as<JsonArray>();
      if (frames.isNull())
        frames = discovered[state]["frames"].to<JsonArray>();
      frames.add(path);
      discovered[state]["frameDurationMs"] = 400;
    }
    const char *period = petBackgroundPeriod(path);
    if (*period && out["backgrounds"][period].isNull())
      out["backgrounds"][period] = path;
  }
  for (JsonPair animation : discovered.as<JsonObject>())
    if (!out["animations"][animation.key().c_str()]["frames"].size())
      out["animations"][animation.key().c_str()] = animation.value();
  for (JsonPairConst animation : manifest["definition"]["animations"].as<JsonObjectConst>())
    if (petStateName(animation.key().c_str()) &&
        !out["animations"][animation.key().c_str()]["frames"].size())
      out["animations"][animation.key().c_str()] = animation.value();
}
} // namespace leap
