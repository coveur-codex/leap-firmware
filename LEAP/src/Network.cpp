#include "Network.h"
#include "NetworkProfiles.h"
#include "ChillPhotos.h"
#include "Core.h"
#include "Media.h"
#include "MemoryUsage.h"
#include "Protocol.h"
#include "QuizTracking.h"
#include <esp_wifi.h>
#include <sys/time.h>
namespace leap {
Network network;
void Network::publishNews(JsonDocument &value) {
  if (!aircraftMutex || !notificationBootEpoch) return;
  value["notificationBootEpoch"] = notificationBootEpoch;
  xSemaphoreTake(aircraftMutex, portMAX_DELAY);
  newsLive.set(value);
  xSemaphoreGive(aircraftMutex);
  newsBootEpoch = notificationBootEpoch;
  ++newsRevision;
}
bool Network::copyNews(JsonDocument &value) {
  if (!aircraftMutex || xSemaphoreTake(aircraftMutex, 0) != pdTRUE) return false;
  bool available = newsLive["articles"].is<JsonArray>();
  if (available) value.set(newsLive);
  xSemaphoreGive(aircraftMutex);
  return available;
}
void Network::publishAircraft(JsonDocument &value) {
  if (!aircraftMutex) return;
  xSemaphoreTake(aircraftMutex, portMAX_DELAY);
  aircraftLive.set(value);
  aircraftLive["receivedMillis"] = millis();
  xSemaphoreGive(aircraftMutex);
}
bool Network::copyAircraft(JsonDocument &value) {
  if (!aircraftMutex || xSemaphoreTake(aircraftMutex, 0) != pdTRUE) return false;
  bool available = aircraftLive["aircraft"].is<JsonArray>();
  if (available) value.set(aircraftLive);
  xSemaphoreGive(aircraftMutex);
  return available;
}
void Network::publishRoom(JsonDocument &value, bool fresh) {
  xSemaphoreTake(aircraftMutex, portMAX_DELAY);
  if (fresh) {
    roomLive.set(value);
    roomLive["receivedMillis"] = millis();
  } else {
    roomLive["notice"] = value["notice"] | "Zimmer nicht erreichbar";
    roomLive.remove("receivedMillis");
  }
  xSemaphoreGive(aircraftMutex);
  ++roomRevision;
}
bool Network::copyRoom(JsonDocument &value) {
  if (!aircraftMutex || xSemaphoreTake(aircraftMutex, 0) != pdTRUE) return false;
  value.set(roomLive);
  xSemaphoreGive(aircraftMutex);
  return true;
}
bool Network::roomCommand(const char *target, const char *action, const char *color) {
  if (!connected || !roomRequests || roomPending.exchange(true)) return false;
  RoomRequest command{};
  strlcpy(command.target,target,sizeof(command.target));
  strlcpy(command.action,action,sizeof(command.action));
  if (color) strlcpy(command.color,color,sizeof(command.color));
  command.created=millis();
  if (xQueueSend(roomRequests,&command,0)==pdTRUE) return true;
  roomPending=false;
  return false;
}
static bool enabled(JsonDocument &s, const char *id) {
  JsonArray pages = s["config"]["pages"].as<JsonArray>();
  for (JsonObject p : pages)
    if (p["id"] == id)
      return p["enabled"] | false;
  return false;
}
static void imageCache(JsonDocument &state, JsonVariantConst value, Transport &net) {
  String url = value | "";
  if (!url.startsWith("/api/"))
    return;
  String cached = state["images"][url] | "";
  if (cached.length() && LittleFS.exists(storage.blob(cached)))
    return;
  String hash = net.cacheImage(url);
  if (hash.length() && Media::validate(storage.blob(hash), url)) {
    if (state["images"].size() >= 40)
      state["images"].clear();
    state["images"][url] = hash;
  }
}
static void pruneRadar() {
  // Keep both persistent snapshots usable; radar never shares the asset blob directory.
  JsonDocument a(&jsonRam), b(&jsonRam);
  storage.readJson("/state0.json", a, SnapshotJsonLimit);
  storage.readJson("/state1.json", b, SnapshotJsonLimit);
  String first = a["content"]["weatherRadar"]["hash"] | "";
  String second = b["content"]["weatherRadar"]["hash"] | "";
  File dir = LittleFS.open("/radar");
  if (!dir || !dir.isDirectory()) return;
  for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
    String name = file.name(), path = file.path();
    file.close();
    if (name != first && name != second) LittleFS.remove(path);
  }
}
bool Network::begin() {
  wifiCommands=xQueueCreate(1,sizeof(WifiCommand));
  if (!wifiCommands) return false;
  if (!quizTracking.begin()) log("QUIZ", "Tracking storage unavailable; queued data retained");
  aircraftMutex = xSemaphoreCreateMutex();
  if (!aircraftMutex) return false;
  base = "/api/v1/devices/" + Transport::encode(deviceSettings.deviceId);
  roomRequests = xQueueCreate(1, sizeof(RoomRequest));
  if (!roomRequests) return false;
  requests = xQueueCreate(3, sizeof(KnowledgeRequest));
  if (!requests)
    return false;
  return xTaskCreatePinnedToCore([](void *p) { static_cast<Network *>(p)->run(); }, "leap-network",
                                 16384, this, 1, &task, 0) == pdPASS;
}
bool Network::knowledge(const String &suffix) {
  if (!requests || suffix.length() > 480 || !suffix.startsWith("/knowledge/"))
    return false;
  KnowledgeRequest request{};
  strlcpy(request.path, suffix.c_str(), sizeof(request.path));
  return xQueueSend(requests, &request, 0) == pdTRUE;
}
bool Network::event(const String &id, const char *name, JsonDocument &state, JsonDocument &response,
                    const String &package, int version, const String &message) {
  JsonDocument body(&jsonRam);
  body["event"] = name;
  body["firmwareVersion"] = FirmwareVersion;
  body["installedAssets"] = state["assets"];
  if (package.length()) {
    body["packageId"] = package;
    body["version"] = version;
  }
  if (message.length())
    body["message"] = message;
  return net.json(base + "/sync/" + Transport::encode(id) + "/events", response, &body);
}
void Network::checkin(JsonDocument &state) {
  JsonDocument request(&jsonRam), response(&jsonRam);
  request["firmwareVersion"] = FirmwareVersion;
  request["wifiRssi"] = WiFi.RSSI();
  request["freeFlash"] = storage.freeBytes();
  writeMemoryUsage(request, memorySnapshot());
  // No fabricated battery percentage: absent ADC wiring means omitted field.
  if (net.json(base + "/checkin", response, &request)) {
    String stamp = response["serverTime"] | "";
    tm utc{};
    if (strptime(stamp.c_str(), "%Y-%m-%dT%H:%M:%S", &utc)) {
      time_t epoch = utcTimestamp(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                                  utc.tm_min, utc.tm_sec);
      timeval tv{epoch, 0};
      settimeofday(&tv, nullptr);
      state["lastTime"] = int64_t(epoch);
      if (!notificationBootEpoch) notificationBootEpoch = int64_t(epoch) - millis() / 1000;
      timeSynced = true;
    }
  }
}
void Network::content(JsonDocument &state) {
  JsonDocument versions(&jsonRam);
  bool hasVersions = net.json(base + "/sync", versions);
  for (const char *key : {"news", "weather", "aircraft", "quiz", "finance"}) {
    if (!enabled(state, key))
      continue;
    String versionKey = String(key) + "Version";
    // Weather/aircraft refresh on every scheduled sync: provider freshness is not
    // reliably represented by the current server's device version counters.
    bool live = String(key) == "finance" || String(key) == "weather" || String(key) == "aircraft" || (String(key) == "news" && !newsFetched);
    if (!live && hasVersions && !state["content"][key].isNull() &&
        state["versions"][versionKey] == versions[versionKey] &&
        state["contentConfig"] == state["config"]["configVersion"])
      continue;
    JsonDocument value(&jsonRam);
    String endpoint = base + "/" + key;
    if (String(key) == "quiz") {
      bool packaged = false;
      for (JsonPair entry : state["assets"].as<JsonObject>())
        packaged |= String(entry.key().c_str()).startsWith("quiz-");
      endpoint += packaged ? "?metadataOnly=true" : "?limitPerCatalog=200";
    }
    if (net.json(endpoint, value)) {
      bool shape = String(key) == "finance" ? value["schemaVersion"] == 1 && value["securities"].is<JsonArray>() && value["securities"].size() <= 48
                   : String(key) == "news"       ? value["articles"].is<JsonArray>()
                   : String(key) == "quiz"     ? value["questions"].is<JsonArray>()
                   : String(key) == "aircraft" ? value["aircraft"].is<JsonArray>()
                                               : value["current"].is<JsonObject>();
      if (shape) {
        if (String(key) == "news") {
          publishNews(value);
          newsFetched = true;
        }
        if (String(key) == "aircraft") publishAircraft(value);
        state["content"][key] = value;
        state["versions"][versionKey] = versions[versionKey];
      }
    }
  }
  state["contentConfig"] = state["config"]["configVersion"];
  if (enabled(state, "weather")) {
    JsonDocument radar(&jsonRam);
    if (net.json(base + "/weather/radar", radar) && radar["available"] == true) {
      String url = radar["image"] | "";
      String oldUrl = state["content"]["weatherRadar"]["image"] | "";
      String hash = state["content"]["weatherRadar"]["hash"] | "";
      if (url.startsWith("/api/v1/assets/weather-radar/") && url.endsWith(".png")) {
        if (url != oldUrl || !digestValid(hash.c_str()) || !LittleFS.exists("/radar/" + hash))
          hash = net.cacheImage(url, true);
        if (hash.length() && Media::validate("/radar/" + hash, url)) {
          radar["hash"] = hash;
          state["content"]["weatherRadar"] = radar;
        } else
          state["content"]["weatherRadar"]["stale"] = true;
      }
    } else {
      state["content"]["weatherRadar"]["stale"] = true;
    }
  }
  int images = 0;
  JsonArray articles = state["content"]["news"]["articles"].as<JsonArray>();
  for (JsonObject article : articles)
    if (images++ < 20)
      imageCache(state, article["image"], net);
  // Retry a missing article image on later syncs, including articles already stored offline.
  if (enabled(state, "knowledge"))
    imageCache(state, state["content"]["knowledge"]["image"], net);
}
bool Network::sync() {
  JsonDocument state(&jsonRam), plan(&jsonRam), body(&jsonRam), reply(&jsonRam);
  storage.load(state);
  if (ota.pendingSync().length()) {
    bool reported =
        event(ota.pendingSync(), ota.rolledBack ? "rollback" : "firmware_confirmed", state, reply);
    if (!reported && net.status != 409 && net.status != 404)
      return false;
    ota.clearReport();
  }
  // Resume an interrupted asset activation/report before superseding its sync.
  JsonDocument previous(&jsonRam);
  if (storage.readJson("/sync-plan.json", previous) && previous["firmware"].isNull() &&
      previous["desiredAssets"].as<JsonVariantConst>() == state["assets"].as<JsonVariantConst>()) {
    String previousId = previous["syncId"] | "";
    if (previousId.length() && event(previousId, "boot_success", state, reply)) {
      if (reply["cleanupAllowed"] == true) {
        storage.writeJson("/cleanup.json", reply);
        assets.cleanup(reply["removeVersions"], state["assets"]);
      }
      event(previousId, "sync_success", state, reply);
    } else if (net.status != 409 && net.status != 404)
      return false;
  }
  body["firmwareVersion"] = FirmwareVersion;
  body["installedAssets"] = state["assets"];
  body["deviceConfigSchema"] = deviceSettings.schema;
  body["freeFlash"] = storage.freeBytes();
  if (!net.json(base + "/sync", plan, &body) || !plan["syncId"].is<const char *>() ||
      !plan["desiredAssets"].is<JsonObject>() || plan["desiredAssets"].size() > MaxPackages)
    return false;
  String syncId = plan["syncId"].as<String>();
  if (!storage.writeJson("/sync-plan.json", plan))
    return false;
  JsonDocument config(&jsonRam);
  if (!net.json(plan["configUrl"].as<String>(), config) ||
      !deviceConfig(config.as<JsonVariantConst>(), deviceSettings.deviceId, plan["configVersion"] | 0))
    return false;
  state["config"] = config;
  // Server flags (especially disabling communication) take effect even when a
  // later optional download fails. Prior assets and cached contents stay intact.
  if (!storage.commit(state)) {
    log("SYNC", "Cannot save config/content snapshot; see STORE size or validation error");
    event(syncId, "update_failed", state, reply, "", 0, "Cannot save config/content snapshot; check serial STORE diagnostics");
    return false;
  }
  checkin(state);
  content(state);
  if (!storage.commit(state)) {
    log("SYNC", "Cannot save config/content snapshot; see STORE size or validation error");
    event(syncId, "update_failed", state, reply, "", 0, "Cannot save config/content snapshot; check serial STORE diagnostics");
    return false;
  }
  if (!plan["firmware"].isNull()) {
    event(syncId, "download_started", state, reply);
    if (!ota.offered(plan["firmware"], syncId, net)) {
      event(syncId, "update_failed", state, reply, "", 0,
            "OTA deferred or verification failed; see serial log / rollback prerequisites");
      return false;
    }
    // Report the offered version, not the currently executing version.
    JsonDocument report(&jsonRam);
    report["event"] = "firmware_installed";
    report["firmwareVersion"] = plan["firmware"]["version"];
    net.json(base + "/sync/" + syncId + "/events", reply, &report);
    Serial.printf("[OTA] Verified inactive image selected: running=%s expected=%s; restarting\n",
                  FirmwareVersion, ota.pendingVersion().c_str());
    delay(100);
    ESP.restart();
    return true;
  }
  if (plan["blockedAssets"].size()) {
    event(syncId, "update_failed", state, reply, "", 0, "Assets require a newer firmware");
    return false;
  }
  for (JsonObject update : plan["assetUpdates"].as<JsonArray>()) {
    String package = update["packageId"] | "";
    int version = update["version"] | 0;
    if (!event(syncId, "download_started", state, reply, package, version)) {
      String reason = "Cannot report download_started; HTTP=" + String(net.status);
      log("SYNC", reason.c_str());
      event(syncId, "update_failed", state, reply, package, version, reason);
      return false;
    }
    if (!assets.install(update, net)) {
      event(syncId, "update_failed", state, reply, package, version, assets.error);
      return false;
    }
  }
  JsonDocument next(&jsonRam);
  next.set(state);
  next["assets"] = plan["desiredAssets"];
  // Full inventory validated before the single atomic snapshot switch.
  for (JsonPair p : next["assets"].as<JsonObject>()) {
    JsonDocument manifest(&jsonRam);
    if (!identifier(p.key().c_str()) ||
        !storage.readJson(storage.package(p.key().c_str(), p.value()), manifest)) {
      event(syncId, "update_failed", state, reply, p.key().c_str(), p.value(),
            "Final inventory manifest missing or invalid");
      return false;
    }
    if (!assets.verify(manifest, true)) {
      event(syncId, "update_failed", state, reply, p.key().c_str(), p.value(), assets.error);
      return false;
    }
  }
  if (!storage.commit(next)) {
    log("SYNC", "Cannot commit final asset inventory");
    event(syncId, "update_failed", state, reply, "", 0, "Cannot commit final asset inventory");
    return false;
  }
  for (JsonObject update : plan["assetUpdates"].as<JsonArray>())
    if (!event(syncId, "asset_installed", next, reply, update["packageId"].as<String>(),
               update["version"]))
      return false;
  // Local integrity/self-test above is complete; boot_success doesn't depend on
  // receiving input or on a second physical reboot for an asset-only change.
  if (!event(syncId, "boot_success", next, reply))
    return false;
  if (reply["cleanupAllowed"] == true) {
    storage.writeJson("/cleanup.json", reply);
    assets.cleanup(reply["removeVersions"], next["assets"]);
  }
  return event(syncId, "sync_success", next, reply);
}
void Network::run() {
  // Chat relay and content workers share the WLAN connection.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true); // Minimum modem sleep: wake at AP DTIM, keep TCP connected.
  WiFi.setAutoReconnect(false); // Profile rotation owns reconnection.
  WiFi.persistent(false); // Credentials live in our atomic NVS record.
  if (!strlen(deviceSettings.ssid))
    log("WIFI", "No SSID configured; check LEAP/LocalConfig.h");
  // Validate cached assets off the UI thread. Input/rendering can start immediately.
  JsonDocument bootState(&jsonRam);
  storage.load(bootState);
  bool valid = storage.ready;
  for (JsonPair p : bootState["assets"].as<JsonObject>()) {
    JsonDocument manifest(&jsonRam);
    if (!storage.readJson(storage.package(p.key().c_str(), p.value()), manifest) ||
        !assets.verify(manifest, true))
      valid = false;
  }
  selfTestPassed = valid;
  selfTestDone = true;
  bootState.clear();
  uint32_t lastConnect = millis() - 30000, lastSync = millis() - SyncInterval;
  uint32_t lastAircraft = millis() - 30000, lastNews = millis();
  uint32_t lastQuizUpload = millis() - 1000, quizUploadInterval = 1000;
  uint32_t interval = 10000;
  int lastWifiStatus = -1;
  size_t wifiIndex = 0;
  String wifiTrialSsid;
  uint32_t wifiGeneration = networkProfiles.wifiGeneration.load();
  const char *lastBlock = nullptr;
  for (;;) {
    WifiCommand command;
    if (xQueueReceive(wifiCommands,&command,0)==pdTRUE) {
      bool saved=command.removeIndex>=0 ? networkProfiles.removeLocal(command.removeIndex) : networkProfiles.saveLocal(command.profile);
      wifiResult=saved ? 2 : 3;
      wifiTrialSsid=saved && command.removeIndex<0 ? command.profile.ssid : "";
    }
    if (wifiGeneration != networkProfiles.wifiGeneration.load()) {
      wifiGeneration = networkProfiles.wifiGeneration.load();
      wifiIndex = 0;
      WiFi.disconnect();
      lastConnect = millis() - 15000;
    }
    int wifiStatus = WiFi.status();
    connected = wifiStatus == WL_CONNECTED;
    if (connected && wifiTrialSsid.length())
      wifiResult=WiFi.SSID()==wifiTrialSsid ? 4 : 5;
    if (wifiStatus != lastWifiStatus) {
      lastWifiStatus = wifiStatus;
      if (connected) {
        lastSync = millis() - SyncInterval;
        interval = 10000;
        Serial.printf("[%10lu] [WIFI    ] Connected ip=%s rssi=%d channel=%d\n", millis(),
                      WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
      } else {
        const char *reason = wifiStatus == WL_NO_SSID_AVAIL ? "SSID not found"
                             : wifiStatus == WL_CONNECT_FAILED ? "Connection failed"
                             : wifiStatus == WL_CONNECTION_LOST ? "Connection lost"
                             : wifiStatus == WL_IDLE_STATUS ? "Connecting"
                                                            : "Disconnected";
        Serial.printf("[%10lu] [WIFI    ] %s status=%d; retry every 30s\n", millis(), reason,
                      wifiStatus);
      }
    }
    if (!connected && elapsed(millis(), lastConnect, 15000)) {
      lastConnect = millis();
      WifiProfile profile;
      if (!networkProfiles.profile(wifiIndex++, profile)) {
        wifiIndex = 1;
        networkProfiles.profile(0, profile);
      }
      if (profile.ssid[0]) {
        WiFi.disconnect();
        WiFi.begin(profile.ssid, profile.password);
        log("WIFI", "Trying saved profile");
      }
    }
    const char *block = !storage.ready ? "Blocked: LittleFS unavailable; see STORE recovery message"
                        : !selfTestPassed ? "Blocked: cached asset self-test failed"
                        : !ota.locallyConfirmed ? "Waiting for local boot confirmation"
                        : !connected ? "Waiting for WLAN"
                                     : nullptr;
    if (block != lastBlock) {
      log("SYNC", block ? block : "Ready; starting scheduled synchronization");
      lastBlock = block;
    }
    if (connected && storage.ready && elapsed(millis(), lastQuizUpload, quizUploadInterval)) {
      JsonDocument attempt(&jsonRam), response(&jsonRam);
      quizUploadInterval = 1000;
      if (quizTracking.front(attempt)) {
        busy = true;
        bool ok = net.json(base + "/quiz-attempts", response, &attempt) &&
                  response["ok"] == true && response["eventId"] == attempt["eventId"];
        if (ok) ok = quizTracking.acknowledge(attempt["eventId"].as<String>());
        if (!ok) {
          quizUploadInterval = 30000;
          Serial.printf("[QUIZ] Upload deferred HTTP=%d; answer retained\n", net.status);
        }
        busy = false;
      }
      lastQuizUpload = millis();
    }
    if (connected && storage.ready && ota.locallyConfirmed &&
        (requested.exchange(false) || elapsed(millis(), lastSync, interval))) {
      busy = true;
      bool ok = sync();
      pruneRadar();
      busy = false;
      lastSync = millis();
      interval = ok ? SyncInterval : std::min(interval * 2, SyncInterval);
      if (!ok && (net.status <= 0 || net.status == 404 || net.status >= 500)) {
        networkProfiles.nextEndpoint();
        interval = 10000;
      }
      if (ok) {
        // Read credentials separately; never persist them in content snapshots.
        JsonDocument config(&jsonRam);
        if (net.json(base + "/network-config", config) && config["schemaVersion"] == 1 &&
            config["deviceId"] == deviceSettings.deviceId && config["revision"].is<uint32_t>() &&
            config["serverUrl"].is<const char *>() && config["wifiProfiles"].is<JsonArray>() &&
            config["wifiProfiles"].size() <= 4) {
          WifiProfile profiles[4]{};
          bool valid = true;
          size_t i=0;
          for (JsonObjectConst p : config["wifiProfiles"].as<JsonArrayConst>()) {
            if (!p["ssid"].is<const char *>() || !p["password"].is<const char *>()) { valid=false; break; }
            const char *ssid = p["ssid"] | "", *password = p["password"] | "";
            if (!ssid[0] || strlen(ssid)>32 || strlen(password)>64) { valid=false; break; }
            strcpy(profiles[i].ssid,ssid); strcpy(profiles[i].password,password);
            if (!profiles[i++].valid()) { valid=false; break; }
          }
          if (valid && !networkProfiles.applyRemote(config["revision"], config["serverUrl"], profiles))
            log("CONFIG", "Network profile update rejected; previous profiles retained");
        }
      }
      log("SYNC", ok ? "Complete" : "Deferred; previous usable data retained");
    }
    // Same network task and atomic offline snapshot as all other content.
    // No HTTP or flash operations take place in rendering/input handlers.
    static uint32_t lastFinance = 0;
    if (connected && storage.ready && ota.locallyConfirmed && elapsed(millis(), lastFinance, 300000)) {
      lastFinance = millis();
      JsonDocument snapshot(&jsonRam);
      if (storage.load(snapshot) && enabled(snapshot, "finance")) {
        JsonDocument value(&jsonRam);
        if (net.json(base + "/finance", value) && value["schemaVersion"] == 1 &&
            value["securities"].is<JsonArray>() && value["securities"].size() <= 48)
          snapshot["content"]["finance"] = value;
        else
          for (JsonObject q : snapshot["content"]["finance"]["securities"].as<JsonArray>()) q["stale"] = true;
        storage.commit(snapshot);
      }
    }
    if (connected && storage.ready && ota.locallyConfirmed && aircraftVisible && elapsed(millis(), lastAircraft, 15000)) {
      lastAircraft = millis();
      JsonDocument value(&jsonRam);
      if (net.json(base + "/aircraft", value) && value["aircraft"].is<JsonArray>())
        publishAircraft(value); // RAM only: do not write flash every thirty seconds.
    }
    if (connected && storage.ready && ota.locallyConfirmed && newsEnabled &&
        notificationBootEpoch && elapsed(millis(), lastNews, 60000)) {
      lastNews = millis();
      JsonDocument value(&jsonRam);
      if (net.json(base + "/news", value) && value["articles"].is<JsonArray>())
        publishNews(value); // RAM only: timely headlines without extra flash writes.
    }
    // Live room state stays in RAM. Commands are never persisted or retried.
    static uint32_t lastRoom=millis()-3000;
    static bool wasRoomVisible=false;
    RoomRequest roomRequest{};
    bool roomWrite=xQueueReceive(roomRequests,&roomRequest,0)==pdTRUE;
    bool visible=roomVisible.load();
    if (roomWrite || (visible && (!wasRoomVisible || elapsed(millis(),lastRoom,3000)))) {
      lastRoom=millis();
      JsonDocument snapshot(&jsonRam), response(&jsonRam), body(&jsonRam);
      bool allowed=connected && storage.load(snapshot) && enabled(snapshot,"room");
      bool fresh=false;
      if (allowed && (!roomWrite || uint32_t(millis()-roomRequest.created)<10000)) {
        if (roomWrite) {
          body["target"]=roomRequest.target;
          body["action"]=roomRequest.action;
          if (roomRequest.color[0]) body["color"]=roomRequest.color;
        }
        fresh=net.json(base+"/room",response,roomWrite ? &body : nullptr,20000) &&
              response["schemaVersion"]==1 && response["lights"].is<JsonArray>() && response["lights"].size()<=2;
      }
      if (!fresh) {
        response["notice"]=response["detail"].is<const char *>() ? response["detail"].as<String>() :
                           (roomWrite ? "Nicht bestätigt · Zustand wird neu geladen" : "Zimmer nicht erreichbar");
      }
      publishRoom(response,fresh);
      if (roomWrite) roomPending=false;
    }
    wasRoomVisible=visible;
    photoInbox.service(net, base, connected);
    KnowledgeRequest request;
    if (xQueueReceive(requests, &request, 0) == pdTRUE && connected) {
      JsonDocument state(&jsonRam);
      storage.load(state);
      if (enabled(state, "knowledge")) {
        JsonDocument response(&jsonRam);
        busy = true;
        if (net.json(base + request.path, response)) {
          bool search = String(request.path).startsWith("/knowledge/search");
          if (search ? response["results"].is<JsonArray>() : response["text"].is<const char *>()) {
            String key = search ? "knowledgeSearch" : "knowledge";
            state["content"][key] = response;
            if (!search)
              imageCache(state, response["image"], net);
            if (storage.commit(state))
              knowledgeRevision.fetch_add(1);
          }
        }
        busy = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
} // namespace leap
