#pragma once
#include <ArduinoJson.h>
#include <string>
namespace leap {
// Same bounded sprite contract as the homeserver; paths refer to hashed package files.
inline bool validChill(JsonVariantConst def, JsonVariantConst files) {
  std::string scene = def["scene"] | "";
  if (scene != "space" && scene != "fire" && scene != "snow" && scene != "loremflickr" &&
      scene != "album")
    return false;
  auto slider = def["slider"];
  if (slider["min"] != 0 ||
      slider["max"] != ((scene == "loremflickr" || scene == "album") ? 300 : 100) ||
      !slider["default"].is<int>() || slider["default"].as<int>() < 0 ||
      slider["default"].as<int>() > ((scene == "loremflickr" || scene == "album") ? 300 : 100))
    return false;
  if (scene == "album") {
    auto photos = def["photos"].as<JsonArrayConst>();
    if (def["minFirmware"] != "1.1.2" || !def["sprites"].is<JsonArrayConst>() ||
        def["sprites"].size() || photos.isNull() || !photos.size() || photos.size() > 24)
      return false;
    for (size_t i = 0; i < photos.size(); ++i) {
      if (!photos[i].is<const char *>())
        return false;
      std::string path = photos[i].as<std::string>();
      if (path.size() < 5 || path.substr(path.size() - 4) != ".jpg")
        return false;
      bool found = false;
      for (JsonObjectConst f : files.as<JsonArrayConst>())
        if (f["path"] == photos[i])
          found = true;
      if (!found)
        return false;
      for (size_t j = 0; j < i; ++j)
        if (photos[i] == photos[j])
          return false;
    }
    return true;
  }
  if (scene == "loremflickr")
    return def["minFirmware"] == "1.1.0" && def["sprites"].is<JsonArrayConst>() &&
           def["sprites"].size() == 0;
  auto sprites = def["sprites"].as<JsonArrayConst>();
  if (sprites.isNull() || !sprites.size() || sprites.size() > 16)
    return false;
  auto exists = [&](JsonVariantConst path) {
    if (!path.is<const char *>())
      return false;
    std::string name = path.as<std::string>();
    if (name.size() < 4 || name.substr(name.size() - 4) != ".png")
      return false;
    for (JsonObjectConst f : files.as<JsonArrayConst>())
      if (f["path"] == path)
        return true;
    return false;
  };
  bool flame = false;
  size_t totalFrames = 0;
  for (JsonObjectConst s : sprites) {
    if (s["file"].isNull() == s["frames"].isNull())
      return false;
    if (s["size"].size() != 2)
      return false;
    for (JsonVariantConst n : s["size"].as<JsonArrayConst>())
      if (!n.is<int>() || n.as<int>() < 1 || n.as<int>() > 142)
        return false;
    auto frames = s["frames"].as<JsonArrayConst>();
    if (!s["file"].isNull()) {
      if (!exists(s["file"]))
        return false;
    } else {
      if (frames.isNull() || !frames.size() || frames.size() > 8)
        return false;
      for (JsonVariantConst f : frames)
        if (!exists(f))
          return false;
    }
    totalFrames += s["file"].isNull() ? frames.size() : 1;
    if (totalFrames > 32)
      return false;
    if (s["role"] == "animation" && frames.size())
      flame = true;
  }
  return scene != "fire" || flame;
}
} // namespace leap
