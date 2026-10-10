#pragma once
#include "Storage.h"
#include <esp_ota_ops.h>
namespace leap {
struct MemoryUsage {
  size_t used = 0;
  size_t total = 0;
};
struct MemorySnapshot {
  MemoryUsage flash, littlefs, psram;
};
inline MemorySnapshot memorySnapshot() {
  MemorySnapshot result;
  // Firmware usage is relative to the active OTA slot, not the entire flash chip.
  static const MemoryUsage flash = []() -> MemoryUsage {
    const esp_partition_t *partition = esp_ota_get_running_partition();
    return partition ? MemoryUsage{ESP.getSketchSize(), partition->size} : MemoryUsage{};
  }();
  result.flash = flash;
  const size_t total = storage.totalSpace.load();
  const size_t free = storage.freeBytes();
  result.littlefs = {total > free ? total - free : 0, total};
  const size_t psramTotal = ESP.getPsramSize();
  const size_t psramFree = ESP.getFreePsram();
  result.psram = {psramTotal > psramFree ? psramTotal - psramFree : 0, psramTotal};
  return result;
}
inline String memoryLabel(const MemoryUsage &usage) {
  if (!usage.total) return "nicht verfügbar";
  String used = String(usage.used / 1048576.0, 1);
  String total = String(usage.total / 1048576.0, 1);
  used.replace(".", ",");
  total.replace(".", ",");
  return used + " / " + total + " MB";
}
inline void writeMemoryUsage(JsonDocument &doc, const MemorySnapshot &snapshot) {
  auto memory = doc["memory"].to<JsonObject>();
  const char *names[] = {"flash", "littlefs", "psram"};
  const MemoryUsage values[] = {snapshot.flash, snapshot.littlefs, snapshot.psram};
  for (int i = 0; i < 3; ++i) {
    memory[names[i]]["used"] = values[i].used;
    memory[names[i]]["total"] = values[i].total;
  }
}
} // namespace leap
