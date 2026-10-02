#pragma once
#include "Core.h"
#include <ArduinoJson.h>
#include <set>
namespace leap {
inline bool suffix(std::string text, const char *ending) {
  for (char &c : text)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  std::string ext = ending;
  return text.size() >= ext.size() && text.compare(text.size() - ext.size(), ext.size(), ext) == 0;
}
// A package version identifies a device representation, not the browser preview.
// For avatars only PNG frames + JSON definitions are relevant to this hardware.
inline bool requiredAssetFile(const std::string &type, const std::string &path) {
  return type != "avatar" || suffix(path, ".png") || suffix(path, ".json");
}
inline std::string avatarFrame(JsonVariantConst manifest, uint32_t now = 0) {
  auto present = [&](const std::string &path) {
    if (!suffix(path, ".png"))
      return false;
    for (JsonObjectConst file : manifest["files"].as<JsonArrayConst>())
      if (file["path"].as<std::string>() == path)
        return true;
    return false;
  };
  auto idle = manifest["definition"]["animations"]["idle"];
  size_t count = 0;
  for (JsonVariantConst frame : idle["frames"].as<JsonArrayConst>())
    if (present(frame.as<std::string>()))
      count++;
  if (count) {
    uint32_t duration = std::max(80, idle["frameDurationMs"] | 120);
    size_t index = (now / duration) % count;
    for (JsonVariantConst frame : idle["frames"].as<JsonArrayConst>())
      if (present(frame.as<std::string>()) && index-- == 0)
        return frame.as<std::string>();
  }
  std::string preview = manifest["definition"]["preview"] | "";
  if (present(preview))
    return preview;
  // The HTTP endpoint's /files/ prefix is not normally part of the manifest path.
  for (const char *path : {"data/pet/idle/frame_01.png", "files/data/pet/idle/frame_01.png"})
    if (present(path))
      return path;
  return ""; // Do not select an arbitrary expression/frame without metadata.
}
// Pure validation shared by target code and host-side protocol tests.
inline bool manifestMetadata(JsonVariantConst m, size_t maxBytes, size_t maxFiles = 256) {
  if (m["schemaVersion"] != 1 || !identifier(m["packageId"] | "") || !m["version"].is<int>() ||
      m["version"].as<int>() < 1 || !m["files"].is<JsonArrayConst>() ||
      m["files"].size() > maxFiles)
    return false;
  if (!m["definition"].is<JsonObjectConst>() || m["definition"]["id"] != m["packageId"] ||
      m["definition"]["version"] != m["version"] || m["definition"]["type"] != m["type"])
    return false;
  std::set<std::string> paths;
  size_t total = 0;
  std::string prefix = "/api/v1/packages/" + m["packageId"].as<std::string>() + "/versions/" +
                       std::to_string(m["version"].as<int>()) + "/files/";
  for (JsonObjectConst f : m["files"].as<JsonArrayConst>()) {
    std::string path = f["path"] | "";
    if (!safePath(path) || !digestValid(f["sha256"] | "") || !f["size"].is<unsigned>() ||
        !paths.insert(path).second || f["url"].as<std::string>() != prefix + path)
      return false;
    if (!requiredAssetFile(m["type"] | "", path))
      continue;
    size_t size = f["size"].as<unsigned>();
    if (size > maxBytes - total)
      return false;
    total += size;
  }
  return paths.count("definition.json");
}
inline bool deviceConfig(JsonVariantConst c, const char *device, int version) {
  if (c["deviceId"] != device || c["configVersion"] != version || !c["age"].is<int>() ||
      c["age"].as<int>() < 0 || c["age"].as<int>() > 120 || !c["pages"].is<JsonArrayConst>() ||
      c["pages"].size() > 32 || !c["communicationEnabled"].is<bool>())
    return false;
  std::set<std::string> ids;
  for (JsonObjectConst p : c["pages"].as<JsonArrayConst>())
    if (!identifier(p["id"] | "") || !p["enabled"].is<bool>() || !p["order"].is<int>() ||
        !ids.insert(p["id"].as<std::string>()).second)
      return false;
  return true;
}
} // namespace leap
