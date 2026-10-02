#include "Storage.h"
#include "Core.h"
#include "Protocol.h"
#include <esp_heap_caps.h>
namespace leap {
RamAllocator jsonRam;
Storage storage;
void *RamAllocator::allocate(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}
void RamAllocator::deallocate(void *p) {
  free(p);
}
void *RamAllocator::reallocate(void *p, size_t n) {
  return heap_caps_realloc(p, n, MALLOC_CAP_8BIT);
}
bool Storage::begin(bool formatRequested) {
  mutex = xSemaphoreCreateMutex();
  if (!mutex || !prefs.begin("leap-store", false))
    return false;
  ready = LittleFS.begin(false);
  if (formatRequested) {
    LittleFS.end();
    log("STORE", "Physical recovery: formatting requested");
    ready = LittleFS.format() && LittleFS.begin(false);
  }
  if (ready) {
    LittleFS.mkdir("/blobs");
    LittleFS.mkdir("/packages");
  }
  log("STORE", ready ? "LittleFS mounted; automatic format disabled"
                     : "Mount failed; USB recovery required");
  return ready;
}
size_t Storage::freeBytes() const {
  return ready ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0;
}
String Storage::package(const String &id, int version) const {
  // Hash of ID is not needed: server IDs fit one path component, checked before use.
  return "/packages/" + id + "-" + String(version) + ".json";
}
bool Storage::parents(const String &path) {
  for (int i = 1; i < (int)path.length(); i++)
    if (path[i] == '/') {
      String dir = path.substring(0, i);
      if (!LittleFS.exists(dir) && !LittleFS.mkdir(dir))
        return false;
    }
  return true;
}
bool Storage::readJson(const String &path, JsonDocument &out) {
  if (!ready)
    return false;
  File f = LittleFS.open(path, "r");
  if (!f || f.size() > JsonLimit) {
    f.close();
    return false;
  }
  auto error = deserializeJson(out, f);
  f.close();
  return !error && !out.overflowed();
}
bool Storage::writeJson(const String &path, JsonDocument &doc) {
  if (!ready || doc.overflowed() || measureJson(doc) > JsonLimit || !parents(path))
    return false;
  String tmp = path + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f)
    return false;
  size_t n = serializeJson(doc, f);
  f.flush();
  f.close();
  if (n != measureJson(doc)) {
    LittleFS.remove(tmp);
    return false;
  }
  // LittleFS rename atomically replaces a destination; never remove it first.
  return LittleFS.rename(tmp, path);
}
bool Storage::validState(JsonDocument &d) {
  if (d["schema"] != 1 || !d["config"].is<JsonObject>() || !d["assets"].is<JsonObject>())
    return false;
  for (JsonPair p : d["assets"].as<JsonObject>()) {
    if (!identifier(p.key().c_str()) || !p.value().is<int>() || p.value().as<int>() < 1)
      return false;
    JsonDocument m(&jsonRam);
    if (!readJson(package(p.key().c_str(), p.value()), m) || m["packageId"] != p.key().c_str() ||
        m["version"] != p.value().as<int>())
      return false;
    for (JsonObject f : m["files"].as<JsonArray>()) {
      if (!digestValid(f["sha256"] | ""))
        return false;
      if (!requiredAssetFile(m["type"] | "", f["path"] | ""))
        continue;
      File file = LittleFS.open(blob(f["sha256"].as<String>()), "r");
      if (!file || file.size() != f["size"].as<size_t>())
        return false;
    }
  }
  return true;
}
bool Storage::load(JsonDocument &out) {
  if (!ready)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int active = prefs.getUChar("active", 0);
  bool ok = readJson(active ? "/state1.json" : "/state0.json", out) && validState(out);
  if (!ok) {
    out.clear();
    ok = readJson(active ? "/state0.json" : "/state1.json", out) && validState(out);
    if (ok) {
      prefs.putUChar("active", 1 - active);
      log("STORE", "Recovered previous snapshot");
    }
  }
  if (!ok) {
    out.clear();
    out["schema"] = 1;
    out["config"].to<JsonObject>();
    out["assets"].to<JsonObject>();
  }
  xSemaphoreGive(mutex);
  return ok;
}
bool Storage::commit(JsonDocument &doc) {
  if (!ready || !validState(doc))
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int next = 1 - prefs.getUChar("active", 0);
  bool ok = writeJson(next ? "/state1.json" : "/state0.json", doc);
  if (ok)
    ok = prefs.putUChar("active", next) == 1;
  if (ok)
    generation.fetch_add(1);
  xSemaphoreGive(mutex);
  return ok;
}
} // namespace leap
