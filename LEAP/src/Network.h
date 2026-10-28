#pragma once
#include "Assets.h"
#include "Ota.h"
#include "NetworkProfiles.h"
#include <WiFi.h>
#include <atomic>
namespace leap {
struct RoomRequest { char target[8], action[8], color[8]; uint32_t created; };
struct KnowledgeRequest {
  char path[512];
};
struct WifiCommand { WifiProfile profile; int removeIndex=-1; };
class Network {
  QueueHandle_t wifiCommands = nullptr;
  std::atomic<int> wifiResult{0};
  QueueHandle_t roomRequests = nullptr;
  JsonDocument roomLive{&jsonRam};
  void publishRoom(JsonDocument &value, bool fresh);
  QueueHandle_t requests = nullptr;
  TaskHandle_t task = nullptr;
  Transport net;
  String base;
  int64_t notificationBootEpoch = 0;
  bool newsFetched = false;
  JsonDocument aircraftLive{&jsonRam}, newsLive{&jsonRam};
  void publishNews(JsonDocument &value);
  SemaphoreHandle_t aircraftMutex = nullptr;
  void publishAircraft(JsonDocument &value);
  bool sync();
  bool event(const String &id, const char *name, JsonDocument &state, JsonDocument &response,
             const String &package = "", int version = 0, const String &message = "");
  void run();
  void checkin(JsonDocument &state);
  void content(JsonDocument &state);

public:
  std::atomic<bool> connected{false}, busy{false}, requested{false};
  std::atomic<bool> timeSynced{false}, selfTestDone{false}, selfTestPassed{false};
  std::atomic<uint32_t> knowledgeRevision{0};
  std::atomic<bool> aircraftVisible{false};
  std::atomic<bool> newsEnabled{false};
  std::atomic<uint32_t> newsRevision{0};
  std::atomic<int64_t> newsBootEpoch{0};
  std::atomic<bool> roomVisible{false}, roomPending{false};
  std::atomic<uint32_t> roomRevision{0};
  bool copyRoom(JsonDocument &value);
  bool roomCommand(const char *target, const char *action, const char *color=nullptr);
  bool copyNews(JsonDocument &value);
  bool copyAircraft(JsonDocument &value);
  bool begin();
  bool saveWifi(const WifiProfile &p) {
    WifiCommand command; command.profile=p;
    wifiResult=1;
    if (!wifiCommands || xQueueSend(wifiCommands,&command,0)!=pdTRUE) { wifiResult=3; return false; }
    return true;
  }
  bool removeWifi(int index) {
    WifiCommand command; command.removeIndex=index;
    wifiResult=1;
    if (!wifiCommands || xQueueSend(wifiCommands,&command,0)!=pdTRUE) { wifiResult=3; return false; }
    return true;
  }
  String wifiNotice() const {
    int result=wifiResult.load();
    if (result==1) return "WLAN wird gespeichert...";
    if (result==3) return "Speichern fehlgeschlagen / Profile voll";
    if (result==4) return "Neues WLAN verbunden";
    if (result==5) return "Anderes Profil aktiv; neues WLAN prüfen";
    if (connected) return "WLAN verbunden";
    return "Suche gespeichertes WLAN (je 15 s)";
  }
  bool knowledge(const String &suffix);
  void requestSync() {
    requested = true;
  }
};
extern Network network;
} // namespace leap
