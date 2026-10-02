#include "Ota.h"
#include "Core.h"
#include <esp_app_format.h>
#include <esp_task_wdt.h>
namespace leap {
Ota ota;
bool Ota::begin() {
  if (!prefs.begin("leap-ota", false))
    return false;
  String target = prefs.getString("version", "");
  rolledBack = target.length() && target != FirmwareVersion;
  esp_ota_img_states_t state;
  const auto *running = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY)
    log("OTA", "Pending verification; self-test required");
  return true;
}
bool Ota::confirm(bool healthy) {
  esp_ota_img_states_t state;
  const auto *running = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY)
    return locallyConfirmed = healthy;
  if (!healthy) {
    log("OTA", "Self-test failed, reverting image");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return false;
  }
  bool ok = esp_ota_mark_app_valid_cancel_rollback() == ESP_OK;
  log("OTA", ok ? "Image confirmed locally" : "Image confirmation failed");
  locallyConfirmed = ok;
  return ok;
}
String Ota::pendingSync() {
  return prefs.getString("sync", "");
}
String Ota::pendingVersion() {
  return prefs.getString("version", "");
}
void Ota::clearReport() {
  prefs.remove("sync");
  prefs.remove("version");
}
bool Ota::offered(JsonObjectConst release, const String &sync, Transport &net) {
#if !LEAP_OTA_ENABLED || !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) ||                        \
    !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  log("OTA", "Offer deferred: tested rollback bootloader is required");
  return false;
#else
  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  size_t size = release["size"] | 0;
  String version = release["version"] | "";
  if (!target || size < sizeof(esp_image_header_t) || size > target->size || !version.length() ||
      version == FirmwareVersion || !meetsVersion(version.c_str(), FirmwareVersion))
    return false;
  esp_ota_handle_t handle;
  if (esp_ota_begin(target, size, &handle) != ESP_OK)
    return false;
  size_t offset = 0;
  bool headerValid = false;
  uint8_t prefix[sizeof(esp_image_header_t)]{};
  size_t prefixSize = 0;
  bool ok = net.download(release["url"].as<String>(), size, release["sha256"].as<String>(),
                         [&](const uint8_t *p, size_t n) {
                           if (prefixSize < sizeof(prefix)) {
                             size_t count = std::min(n, sizeof(prefix) - prefixSize);
                             memcpy(prefix + prefixSize, p, count);
                             prefixSize += count;
                             if (prefixSize == sizeof(prefix)) {
                               esp_image_header_t header;
                               memcpy(&header, prefix, sizeof(header));
                               headerValid = header.magic == ESP_IMAGE_HEADER_MAGIC &&
                                             header.chip_id == ESP_CHIP_ID_ESP32S3;
                               if (!headerValid)
                                 return false;
                             }
                           }
                           offset += n;
                           return esp_ota_write(handle, p, n) == ESP_OK;
                         });
  if (!ok || !headerValid || offset != size) {
    esp_ota_abort(handle);
    return false;
  }
  if (esp_ota_end(handle) != ESP_OK)
    return false; // validates full ESP image, not just magic
  // Journal BEFORE changing boot selection. A power loss here reports a harmless rollback.
  if (prefs.putString("sync", sync) != sync.length() ||
      prefs.putString("version", version) != version.length())
    return false;
  if (esp_ota_set_boot_partition(target) != ESP_OK) {
    clearReport();
    return false;
  }
  return true;
#endif
}
} // namespace leap

// Arduino core otherwise validates a pending image before setup() executes.
extern "C" bool verifyRollbackLater() {
  return true;
}
