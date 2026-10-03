#include "src/Audio.h"
#include "src/Config.h"
#include "src/Input.h"
#include "src/Motion.h"
#include "src/Network.h"
#include "src/Radio.h"
#include "src/Storage.h"
#include "src/Ui.h"
#include <esp_psram.h>
#include <esp_task_wdt.h>
#include <sys/time.h>
using namespace leap;
Input input;
bool bootHealthy = false, bootConfirmed = false;
uint32_t bootAt = 0, lastDiag = 0;
void setup() {
  Serial.begin(115200); // Never wait for a USB host.
  Serial.printf("\n[BOOT] LEAP %s reset=%d flash=%u psram=%u\n", FirmwareVersion,
                esp_reset_reason(), unsigned(ESP.getFlashChipSize()), unsigned(ESP.getPsramSize()));
  input.begin();
  ota.begin();
  // Explicit destructive recovery requires BOTH centres held throughout 3s.
  bool format = digitalRead(hw::LeftKeys[4]) == LOW && digitalRead(hw::RightKeys[4]) == LOW;
  if (format) {
    log("RECOVERY", "Release either centre within 3s to cancel filesystem format");
    for (int i = 0; i < 300; i++) {
      if (digitalRead(hw::LeftKeys[4]) || digitalRead(hw::RightKeys[4])) {
        format = false;
        break;
      }
      delay(10);
    }
  }
  bool mounted = storage.begin(format);
  setenv("TZ", LEAP_TIMEZONE, 1);
  tzset();
  JsonDocument state(&jsonRam);
  storage.load(state);
  time_t saved = state["lastTime"] | int64_t(0);
  if (saved > 1700000000) {
    timeval tv{saved, 0};
    settimeofday(&tv, nullptr);
    log("CLOCK", "Restored last known time; power-off duration unknown");
  }
  bool radioReady = radio.begin();
  bool soundReady = audio.begin();
  motion.begin();
  bool displayReady = ui.begin();
  bool workerReady = network.begin();
  bootHealthy = mounted && displayReady && workerReady &&
                ESP.getFlashChipSize() == 16 * 1024 * 1024 &&
                esp_psram_get_size() == 8 * 1024 * 1024;
  // Network task validates cached assets while this task starts rendering/input.
  Serial.printf("[BOOT] selftest=%d radio=%d audio=%d\n", bootHealthy, radioReady, soundReady);
  esp_task_wdt_config_t watchdog{.timeout_ms = 10000, .idle_core_mask = 0, .trigger_panic = true};
  if (esp_task_wdt_reconfigure(&watchdog) == ESP_ERR_INVALID_STATE)
    esp_task_wdt_init(&watchdog);
  esp_task_wdt_add(nullptr);
  bootAt = millis();
}
void loop() {
  InputEvent e;
  for (int i = 0; i < 10 && input.poll(e); i++)
    ui.input(e);
  motion.poll();
  ui.tick();
  esp_task_wdt_reset();
  if (!bootConfirmed &&
      (network.selfTestDone || !bootHealthy || elapsed(millis(), bootAt, 60000)) &&
      elapsed(millis(), bootAt, 5000)) {
    bootConfirmed = true;
    ota.confirm(bootHealthy && network.selfTestDone && network.selfTestPassed);
  }
  if (elapsed(millis(), lastDiag, 60000)) {
    lastDiag = millis();
    Serial.printf("[HEALTH] heap=%u min=%u psram=%u fsFree=%u wifiConnected=%d fsReady=%d bootConfirmed=%d\n",
                  unsigned(ESP.getFreeHeap()), unsigned(ESP.getMinFreeHeap()),
                  unsigned(ESP.getFreePsram()), unsigned(storage.freeBytes()),
                  network.connected.load(), storage.ready, ota.locallyConfirmed.load());
  }
  delay(5);
}
