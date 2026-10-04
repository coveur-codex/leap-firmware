#include "Storage.h"
#include "Core.h"
#include "Protocol.h"
#include "StorageRecovery.h"
#include <esp_heap_caps.h>
#include <esp_partition.h>
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
  if (!mutex || !prefs.begin("leap-store", false)) {
    log("STORE", "Mutex or NVS initialization failed");
    return false;
  }
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!partition) {
    log("STORE", "Missing spiffs partition; upload the custom partition table over USB");
    return false;
  }
  ready = LittleFS.begin(false, "/littlefs", 10, "spiffs");
  bool blank = !ready && !formatRequested &&
               partitionErased(partition->size, [&](size_t offset, uint8_t *data, size_t size) {
                 bool ok = esp_partition_read(partition, offset, data, size) == ESP_OK;
                 if (offset % 4096 == 0)
                   delay(1);
                 return ok;
               });
  if (formatRequested || blank) {
    LittleFS.end();
    log("STORE", blank ? "First boot: initializing fully erased LittleFS partition"
                       : "Physical recovery: formatting requested");
    ready = LittleFS.format() && LittleFS.begin(false, "/littlefs", 10, "spiffs");
  }
  if (ready) {
    ready = (LittleFS.exists("/blobs") || LittleFS.mkdir("/blobs")) &&
            (LittleFS.exists("/packages") || LittleFS.mkdir("/packages"));
  }
  log("STORE", ready ? "LittleFS ready; existing data protected"
                     : "LittleFS unavailable; data retained. Hold BOTH centres at boot for 3s to erase");
  refreshSpace();
  return ready;
}
size_t Storage::freeBytes() const {
  return freeSpace.load();
}
void Storage::refreshSpace() {
  // usedBytes() scans LittleFS; never perform that scan in the UI/health loop.
  freeSpace = ready ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0;
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
bool Storage::readJson(const String &path, JsonDocument &out, size_t limit) {
  uint32_t started = millis();
  if (!ready)
    return false;
  File f = LittleFS.open(path, "r");
  if (!f || f.size() > limit) {
    Serial.printf("[STORE] JSON read rejected path=%s bytes=%u limit=%u\n", path.c_str(),
                  unsigned(f ? f.size() : 0), unsigned(limit));
    f.close();
    return false;
  }
  // Stream parsing otherwise performs a LittleFS read for each individual byte.
  size_t size = f.size();
  char *buffer = static_cast<char *>(jsonRam.allocate(size + 1));
  if (!buffer) {
    f.close();
    return false;
  }
  size_t read = 0;
  while (read < size) {
    size_t n = f.read(reinterpret_cast<uint8_t *>(buffer) + read,
                      std::min(StorageBlockSize, size - read));
    if (!n) break;
    read += n;
    vTaskDelay(1);
  }
  f.close();
  uint32_t readAt = millis();
  // Const input makes ArduinoJson own its strings after the buffer is freed.
  auto error = deserializeJson(out, static_cast<const char *>(buffer), read);
  jsonRam.deallocate(buffer);
  if (millis() - started >= 200)
    Serial.printf("[STORE] JSON timing path=%s bytes=%u IO=%lu ms parse=%lu ms\n",
                  path.c_str(), unsigned(size), readAt - started, millis() - readAt);
  bool ok = read == size && !error && !out.overflowed();
  if (!ok)
    Serial.printf("[STORE] JSON parse/read failed path=%s read=%u/%u error=%s overflow=%d\n",
                  path.c_str(), unsigned(read), unsigned(size), error.c_str(), out.overflowed());
  return ok;
}
bool Storage::writeJson(const String &path, JsonDocument &doc, size_t limit) {
  size_t size = measureJson(doc);
  if (!ready || doc.overflowed() || size > limit || !parents(path)) {
    Serial.printf("[STORE] JSON write rejected path=%s bytes=%u limit=%u ready=%d overflow=%d\n",
                  path.c_str(), unsigned(size), unsigned(limit), ready, doc.overflowed());
    return false;
  }
  String tmp = path + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f)
    return false;
  char *buffer = static_cast<char *>(jsonRam.allocate(size + 1));
  if (!buffer) {
    f.close();
    LittleFS.remove(tmp);
    return false;
  }
  size_t encoded = serializeJson(doc, buffer, size + 1);
  size_t n = 0;
  while (encoded == size && n < size) {
    size_t written = f.write(reinterpret_cast<const uint8_t *>(buffer) + n,
                             std::min(StorageBlockSize, size - n));
    if (!written) break;
    n += written;
    vTaskDelay(1);
  }
  jsonRam.deallocate(buffer);
  f.flush();
  f.close();
  if (n != size) {
    LittleFS.remove(tmp);
    return false;
  }
  // LittleFS rename atomically replaces a destination; never remove it first.
  bool ok = LittleFS.rename(tmp, path);
  refreshSpace();
  return ok;
}
bool Storage::validState(JsonDocument &d, JsonDocument &inventory) {
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
    inventory[p.key().c_str()] = m;
  }
  return !inventory.overflowed();
}
bool Storage::manifest(const String &id, int version, JsonDocument &out) {
  if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    bool found = cachedLoaded && cachedManifests && (*cachedManifests)[id]["version"] == version;
    if (found) out.set((*cachedManifests)[id]);
    xSemaphoreGive(mutex);
    if (found) return !out.overflowed();
  }
  return readJson(package(id, version), out);
}
std::shared_ptr<const JsonDocument> Storage::manifestView() {
  if (!mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return {};
  auto view = cachedManifests;
  xSemaphoreGive(mutex);
  return view;
}
bool Storage::load(JsonDocument &out, TickType_t wait, bool *busy, JsonDocument *inventory,
                   std::shared_ptr<const JsonDocument> *view) {
  if (busy) *busy = false;
  if (!ready)
    return false;
  if (xSemaphoreTake(mutex, wait) != pdTRUE) {
    if (busy) *busy = true;
    return false;
  }
  if (cachedLoaded) {
    uint32_t copyAt = millis();
    out.set(cached);
    if (inventory && cachedManifests) inventory->set(*cachedManifests);
    if (view) *view = cachedManifests;
    xSemaphoreGive(mutex);
    if (millis() - copyAt >= 200)
      Serial.printf("[STORE] RAM snapshot copy %lu ms\n", millis() - copyAt);
    return !out.overflowed() && (!inventory || !inventory->overflowed());
  }
  JsonDocument manifests(&jsonRam);
  int active = prefs.getUChar("active", 0);
  uint32_t started = millis();
  bool parsed = readJson(active ? "/state1.json" : "/state0.json", out, SnapshotJsonLimit);
  uint32_t parsedAt = millis();
  bool ok = parsed && validState(out, manifests);
  Serial.printf("[STORE] Boot snapshot read/parse=%lu ms inventory validation=%lu ms\n",
                parsedAt - started, millis() - parsedAt);
  if (!ok) {
    out.clear();
    manifests.clear();
    ok = readJson(active ? "/state0.json" : "/state1.json", out, SnapshotJsonLimit) &&
         validState(out, manifests);
    if (ok) {
      prefs.putUChar("active", 1 - active);
      log("STORE", "Recovered previous snapshot");
    }
  }
  if (!ok) {
    manifests.clear();
    out.clear();
    out["schema"] = 1;
    out["config"].to<JsonObject>();
    out["assets"].to<JsonObject>();
  }
  uint32_t copyAt = millis();
  cached.set(out);
  Serial.printf("[STORE] Boot RAM snapshot copy %lu ms\n", millis() - copyAt);
  cachedManifests = std::make_shared<JsonDocument>(std::move(manifests));
  cachedLoaded = !cached.overflowed();
  if (inventory) inventory->set(*cachedManifests);
  if (view) *view = cachedManifests;
  xSemaphoreGive(mutex);
  return ok;
}
bool Storage::commit(JsonDocument &doc) {
  JsonDocument manifests(&jsonRam), published(&jsonRam);
  if (!ready || doc.overflowed() || measureJson(doc) > SnapshotJsonLimit ||
      !validState(doc, manifests)) {
    Serial.printf("[STORE] Snapshot rejected bytes=%u limit=%u overflow=%d; previous retained\n",
                  unsigned(measureJson(doc)), unsigned(SnapshotJsonLimit), doc.overflowed());
    return false;
  }
  published.set(doc);
  if (published.overflowed()) return false;
  // Network task is the sole writer. Flash work stays outside the publication
  // mutex so the UI can keep copying the previous RAM snapshot while it is saved.
  int next = 1 - prefs.getUChar("active", 0);
  bool ok = writeJson(next ? "/state1.json" : "/state0.json", doc, SnapshotJsonLimit);
  if (ok)
    ok = prefs.putUChar("active", next) == 1;
  if (ok) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    cached = std::move(published);
    cachedManifests = std::make_shared<JsonDocument>(std::move(manifests));
    cachedLoaded = true;
    generation.fetch_add(1);
    xSemaphoreGive(mutex);
  } else
    log("STORE", "Snapshot write/activation failed; previous RAM snapshot retained");
  return ok;
}
} // namespace leap
