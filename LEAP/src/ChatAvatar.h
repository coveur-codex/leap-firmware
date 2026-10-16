#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include <string>
namespace leap {
inline bool validAvatarImage(const char *id) {
  if (!id)
    return false;
  if (!*id)
    return true; // Older messages have no portrait snapshot.
  if (std::strlen(id) != 64)
    return false;
  for (size_t i = 0; i < 64; ++i)
    if (!(id[i] >= '0' && id[i] <= '9') && !(id[i] >= 'a' && id[i] <= 'f'))
      return false;
  return true;
}
inline std::string chatAvatarBlob(JsonVariantConst manifest, const char *id) {
  if (!validAvatarImage(id) || !*id)
    return "";
  std::string path = std::string(id) + ".png";
  for (JsonObjectConst file : manifest["files"].as<JsonArrayConst>())
    if (file["path"] == path && file["sha256"] == id)
      return id;
  return "";
}
} // namespace leap
