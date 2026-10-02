#pragma once
#include "Config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <atomic>
namespace leap {
// ArduinoJson uses PSRAM for large documents, falling back to internal RAM.
class RamAllocator : public ArduinoJson::Allocator {
public:
  void *allocate(size_t n) override;
  void deallocate(void *p) override;
  void *reallocate(void *p, size_t n) override;
};
extern RamAllocator jsonRam;
class Storage {
  Preferences prefs;
  SemaphoreHandle_t mutex = nullptr;
  bool validState(JsonDocument &doc);

public:
  std::atomic<uint32_t> generation{0};
  bool ready = false;
  bool begin(bool formatRequested = false);
  bool load(JsonDocument &out);
  bool commit(JsonDocument &doc);
  bool readJson(const String &path, JsonDocument &out);
  bool writeJson(const String &path, JsonDocument &doc);
  bool parents(const String &path);
  String blob(const String &hash) const {
    return "/blobs/" + hash;
  }
  String package(const String &id, int version) const;
  size_t freeBytes() const;
};
extern Storage storage;
} // namespace leap
