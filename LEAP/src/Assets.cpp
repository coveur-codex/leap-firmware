#include "Assets.h"
#include "Audio.h"
#include "Core.h"
#include "Media.h"
#include "Protocol.h"
#include <mbedtls/sha256.h>
#include <set>
#include <vector>
namespace leap {
Assets assets;
bool Assets::fail(JsonDocument &m, const String &reason, const String &path) {
  error = String(m["packageId"] | "unknown-package") + " v" + String(m["version"] | 0) +
          (path.length() ? " / " + path : "") + ": " + reason;
  error.replace("\n", " ");
  error.replace("\r", " ");
  if (error.length() > 1800) error.remove(1800);
  log("ASSETS", error.c_str());
  return false;
}
static bool fileHash(const String &path, const String &expected, size_t size) {
  File f = LittleFS.open(path, "r");
  if (!f || f.size() != size)
    return false;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  uint8_t buffer[2048], digest[32];
  size_t done = 0;
  while (f.available()) {
    int n = f.read(buffer, sizeof(buffer));
    if (n <= 0)
      break;
    done += n;
    mbedtls_sha256_update(&ctx, buffer, n);
    vTaskDelay(1);
  }
  f.close();
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  char hex[65];
  for (int i = 0; i < 32; i++)
    snprintf(hex + 2 * i, 3, "%02x", digest[i]);
  return done == size && expected == hex;
}
String Assets::resolve(const String &id, int version, const String &path) {
  if (!identifier(id.c_str()) || !safePath(path.c_str()))
    return "";
  JsonDocument m(&jsonRam);
  if (!storage.readJson(storage.package(id, version), m))
    return "";
  for (JsonObject f : m["files"].as<JsonArray>())
    if (f["path"] == path)
      return storage.blob(f["sha256"].as<String>());
  return "";
}
bool Assets::definition(const String &id, int version, JsonDocument &out) {
  JsonDocument m(&jsonRam);
  if (!storage.readJson(storage.package(id, version), m))
    return false;
  out.set(m["definition"]);
  return !out.overflowed();
}
bool Assets::validDefinition(JsonDocument &m) {
  auto def = m["definition"].as<JsonObject>();
  if (def.isNull() || def["id"] != m["packageId"] || def["version"] != m["version"])
    return fail(m, "Definition identity/version mismatch");
  auto exists = [&](const String &name) {
    for (JsonObject f : m["files"].as<JsonArray>())
      if (f["path"] == name)
        return true;
    return false;
  };
  for (const char *key : {"preview", "questionsFile", "messagesFile"})
    if (!def[key].isNull() && (!def[key].is<const char *>() || !exists(def[key].as<String>())))
      return fail(m, "Missing or invalid definition reference: " + String(key));
  JsonObject animations = def["animations"].as<JsonObject>();
  for (JsonPair pair : animations) {
    JsonArray frames = pair.value()["frames"].as<JsonArray>();
    if (frames.isNull() || !frames.size() || int(pair.value()["frameDurationMs"] | 100) < 1)
      return fail(m, "Invalid animation: " + String(pair.key().c_str()));
    for (JsonVariant f : frames)
      if (!f.is<const char *>() || !exists(f.as<String>()))
        return fail(m, "Missing animation frame");
  }
  if (!def["minFirmware"].isNull() && !meetsVersion(FirmwareVersion, def["minFirmware"] | ""))
    return fail(m, "Minimum firmware requirement not met");
  String kind = m["type"] | "";
  if (kind == "avatar" && avatarFrame(m.as<JsonVariantConst>()).empty())
    return fail(m, "No supported PNG idle/preview frame");
  if (kind == "quiz" || kind == "communication") {
    String key = kind == "quiz" ? "questionsFile" : "messagesFile";
    if (!def[key].is<const char *>())
      return fail(m, "Missing catalog path: " + key);
    String path;
    for (JsonObject f : m["files"].as<JsonArray>())
      if (f["path"] == def[key])
        path = storage.blob(f["sha256"].as<String>());
    JsonDocument content(&jsonRam);
    if (!storage.readJson(path, content))
      return fail(m, "Catalog JSON unreadable or invalid", def[key].as<String>());
    if (kind == "quiz") {
      if (!content["questions"].is<JsonArray>())
        return fail(m, "Quiz questions must be an array", def[key].as<String>());
      for (JsonObject q : content["questions"].as<JsonArray>()) {
        if (!q["q"].is<const char *>() || q["a"].size() != 4 || !q["minAge"].is<int>())
          return fail(m, "Quiz question requires q, four answers and integer minAge", def[key].as<String>());
        JsonArray answers = q["a"].as<JsonArray>();
        for (JsonVariant a : answers)
          if (!a.is<const char *>())
            return fail(m, "Quiz answer must be text", def[key].as<String>());
      }
    } else {
      if (content["schemaVersion"] != 1 || !content["messages"].is<JsonArray>())
        return fail(m, "Communication catalog requires schemaVersion=1 and messages array", def[key].as<String>());
      std::set<std::string> ids;
      for (JsonObject msg : content["messages"].as<JsonArray>()) {
        String id = msg["id"] | "", text = msg["text"] | "";
        if (id.length() != 36 || !identifier(id.c_str()) || !text.length() || text.length() > 480 ||
            !ids.insert(id.c_str()).second)
          return fail(m, "Invalid/duplicate message ID or message text", def[key].as<String>());
      }
    }
  }
  return true;
}
bool Assets::verify(JsonDocument &m, bool hashes) {
  error = "";
  if (!manifestMetadata(m.as<JsonVariantConst>(), LittleFS.totalBytes()))
    return fail(m, "Invalid manifest metadata or package capacity");
  if (m["schemaVersion"] != 1 || !identifier(m["packageId"] | "") || !m["version"].is<int>() ||
      int(m["version"]) < 1 || !m["files"].is<JsonArray>() || m["files"].size() > MaxFiles)
    return fail(m, "Invalid manifest schema or file count");
  std::set<std::string> paths;
  for (JsonObject f : m["files"].as<JsonArray>()) {
    String path = f["path"] | "", hash = f["sha256"] | "";
    if (!safePath(path.c_str()) || !digestValid(hash.c_str()) || !f["size"].is<unsigned>() ||
        !paths.insert(path.c_str()).second)
      return fail(m, "Invalid or duplicate file entry", path);
    if (!requiredAssetFile(m["type"] | "", path.c_str()))
      continue;
    File file = LittleFS.open(storage.blob(hash), "r");
    if (!file || file.size() != f["size"].as<size_t>())
      return fail(m, "Cached file missing or size mismatch", path);
    file.close();
    if (hashes) {
      if (!fileHash(storage.blob(hash), hash, f["size"]))
        return fail(m, "Cached SHA-256 mismatch", path);
      String ext = path;
      ext.toLowerCase();
      if (ext.endsWith(".png") || ext.endsWith(".jpg") || ext.endsWith(".jpeg")) {
        uint32_t width = 0, height = 0;
        if (m["type"] == "avatar" && !Media::avatarPng(storage.blob(hash), &width, &height))
          return fail(m, "Avatar PNG must be 80x80; header=" + String(width) + "x" + String(height), path);
        if (!Media::validate(storage.blob(hash), path))
          return fail(m, "Image decoder rejected file", path);
      } else if (ext.endsWith(".wav")) {
        if (!Audio::validateWav(storage.blob(hash)))
          return fail(m, "Unsupported or invalid PCM WAV", path);
      } else if (ext.endsWith(".json")) {
        JsonDocument d(&jsonRam);
        if (!storage.readJson(storage.blob(hash), d))
          return fail(m, "Invalid JSON file", path);
      } else {
        log("ASSETS", "Unsupported media format; keeping previous package");
        return fail(m, "Unsupported media format", path);
      }
    }
  }
  if (!paths.count("definition.json"))
    return fail(m, "Missing definition.json");
  return validDefinition(m);
}
bool Assets::install(JsonObjectConst update, Transport &net) {
  error = "";
  String id = update["packageId"] | "";
  int version = update["version"] | 0;
  if (!identifier(id.c_str()) || version < 1) {
    error = "Invalid package update identity/version";
    log("ASSETS", error.c_str());
    return false;
  }
  Serial.printf("[%10lu] [ASSETS  ] Installing %s v%d\n", millis(), id.c_str(), version);
  JsonDocument m(&jsonRam);
  if (!net.json(update["manifestUrl"].as<String>(), m) || m["packageId"] != id ||
      m["version"] != version || m["schemaVersion"] != 1 || !m["files"].is<JsonArray>() ||
      m["files"].size() > MaxFiles)
    return fail(m, "Manifest request/schema mismatch for " + id + " HTTP=" + String(net.status));
  if (!manifestMetadata(m.as<JsonVariantConst>(), LittleFS.totalBytes()))
    return fail(m, "Invalid manifest metadata or package capacity");
  if (m["type"] == "avatar" && avatarFrame(m.as<JsonVariantConst>()).empty()) {
    log("ASSETS", "Avatar needs an 80x80 PNG idle/preview; SVG is never downloaded");
    return fail(m, "No supported PNG idle/preview frame");
  }
  size_t needed = 0;
  std::set<std::string> paths;
  for (JsonObject f : m["files"].as<JsonArray>()) {
    String path = f["path"] | "", hash = f["sha256"] | "";
    if (!safePath(path.c_str()) || !digestValid(hash.c_str()) || !f["size"].is<unsigned>() ||
        !paths.insert(path.c_str()).second)
      return fail(m, "Invalid or duplicate file entry", path);
    if (!requiredAssetFile(m["type"] | "", path.c_str()))
      continue;
    size_t size = f["size"];
    if (fileHash(storage.blob(hash), hash, size))
      continue;
    if (size > storage.freeBytes() || needed > storage.freeBytes() - size)
      return fail(m, "Insufficient staging capacity", path);
    needed += size;
  }
  if (needed + ReserveBytes + measureJson(m) > storage.freeBytes()) {
    log("ASSETS", "Insufficient staging space; keeping active packages");
    return fail(m, "Insufficient staging capacity including reserve");
  }
  for (JsonObject f : m["files"].as<JsonArray>()) {
    if (!requiredAssetFile(m["type"] | "", f["path"] | ""))
      continue;
    String hash = f["sha256"], dest = storage.blob(hash);
    size_t size = f["size"];
    if (fileHash(dest, hash, size))
      continue;
    String temp = dest + ".part";
    File file = LittleFS.open(temp, "w");
    if (!file)
      return fail(m, "Cannot open staging file", f["path"] | "");
    String url = f["url"] | "";
    url.replace(" ", "%20");
    bool ok;
    if (size == 0) {
      ok = hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
      if (!ok) net.error = "Empty file has invalid SHA-256";
    }
    else
      ok = net.download(url, size, hash,
                        [&](const uint8_t *p, size_t n) { return file.write(p, n) == n; });
    file.flush();
    file.close();
    if (!ok) {
      LittleFS.remove(temp);
      return fail(m, "Download failed: " + net.error, f["path"] | "");
    }
    if (!LittleFS.rename(temp, dest))
      return fail(m, "Staging rename failed", f["path"] | "");
  }
  if (!verify(m, true))
    return false;
  // The downloaded definition must equal the embedded (unhashed) metadata.
  JsonDocument def(&jsonRam);
  String path;
  for (JsonObject f : m["files"].as<JsonArray>())
    if (f["path"] == "definition.json")
      path = storage.blob(f["sha256"].as<String>());
  if (!storage.readJson(path, def) ||
      def.as<JsonVariantConst>() != m["definition"].as<JsonVariantConst>())
    return fail(m, "Downloaded definition.json differs from manifest or cannot be parsed", "definition.json");
  if (!storage.writeJson(storage.package(id, version), m))
    return fail(m, "Cannot save package manifest");
  log("ASSETS", "Package verified and staged; awaiting full inventory activation");
  return true;
}
void Assets::cleanup(JsonArrayConst removals, JsonObjectConst active) {
  for (JsonObjectConst item : removals) {
    String id = item["packageId"] | "";
    int version = item["version"] | 0;
    if (identifier(id.c_str()) && version > 0 && active[id].as<int>() != version)
      LittleFS.remove(storage.package(id, version));
  }
  // Server authorized cleanup; scan ALL retained/staged manifests before
  // deleting content-addressed files because packages can share the same blob.
  std::set<std::string> retained;
  File directory = LittleFS.open("/packages");
  File entry = directory.openNextFile();
  bool valid = true;
  while (entry) {
    String path = entry.path();
    entry.close();
    JsonDocument m(&jsonRam);
    if (!storage.readJson(path, m)) {
      valid = false;
      break;
    }
    for (JsonObject f : m["files"].as<JsonArray>())
      retained.insert(f["sha256"] | "");
    entry = directory.openNextFile();
  }
  directory.close();
  if (!valid)
    return;
  // News/knowledge image caches are also live references in both snapshots.
  for (const char *path : {"/state0.json", "/state1.json"}) {
    JsonDocument s(&jsonRam);
    if (storage.readJson(path, s))
      for (JsonPair p : s["images"].as<JsonObject>())
        retained.insert(p.value().as<std::string>());
  }
  directory = LittleFS.open("/blobs");
  entry = directory.openNextFile();
  std::vector<String> remove;
  while (entry) {
    String path = entry.path(), name = entry.name();
    entry.close();
    if (!retained.count(name.c_str()))
      remove.push_back(path);
    entry = directory.openNextFile();
  }
  directory.close();
  for (const String &path : remove)
    LittleFS.remove(path);
}
} // namespace leap
