#include "Radio.h"
#include "Assets.h"
#include "Config.h"
#include <WiFi.h>
#include <esp_wifi.h>
namespace leap {
Radio radio;
void Radio::receive(const esp_now_recv_info_t *info, const uint8_t *bytes, int size) {
  if (!radio.enabled || !radio.incoming || size != sizeof(ChatPacket))
    return;
  ChatPacket packet;
  memcpy(&packet, bytes, sizeof(packet));
  if (!validPacket(packet, size) || memcmp(info->src_addr, packet.sender, 6))
    return;
  xQueueSend(radio.incoming, &packet, 0); // WiFi callback: no JSON, display, IO or relay.
}
bool Radio::begin() {
  incoming = xQueueCreate(12, sizeof(ChatPacket));
  if (!incoming)
    return false;
  boot = esp_random();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_channel(deviceSettings.radioChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (esp_now_init() != ESP_OK || esp_now_register_recv_cb(receive) != ESP_OK)
    return false;
  esp_now_peer_info_t peer{};
  memset(peer.peer_addr, 255, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  ready = esp_now_add_peer(&peer) == ESP_OK;
  log("RADIO", ready ? "ESP-NOW group broadcast ready" : "Peer setup failed");
  return ready;
}
void Radio::configure(JsonDocument &state) {
  enabled = false;
  templates.clear();
  version = state["assets"]["communication-messages"] | 0;
  name = state["config"]["avatarName"] | "";
  if (!name.length())
    name = state["config"]["name"] | "LEAP";
  if (state["config"]["communicationEnabled"] == true && version) {
    JsonDocument def(&jsonRam);
    if (assets.definition("communication-messages", version, def)) {
      String path =
          assets.resolve("communication-messages", version, def["messagesFile"] | "messages.json");
      if (storage.readJson(path, templates) && templates["schemaVersion"] == 1)
        enabled = true;
    }
  }
  if (!enabled) {
    count = 0;
    if (incoming)
      xQueueReset(incoming);
    revision++;
  }
}
void Radio::poll() {
  if (!enabled)
    return;
  ChatPacket packet;
  int budget = 4;
  while (budget-- && xQueueReceive(incoming, &packet, 0) == pdTRUE) {
    if (packet.templates != version || !memcmp(packet.sender, mac, 6) || !seen.accept(packet))
      continue;
    String text;
    for (JsonObjectConst m : messages())
      if (m["id"] == packet.message) {
        text = m["text"].as<String>();
        break;
      }
    if (!text.length())
      continue;
    if (count == 8) {
      for (int i = 1; i < 8; i++)
        history[i - 1] = history[i];
      count--;
    }
    history[count++] = {String(packet.name), text, millis()};
    revision++;
  }
}
bool Radio::send(size_t index) {
  if (!ready || !enabled || index >= messages().size() || !elapsed(millis(), lastSend, 1000))
    return false;
  ChatPacket p;
  p.boot = boot;
  p.sequence = ++sequence;
  p.templates = version;
  memcpy(p.sender, mac, 6);
  strlcpy(p.message, messages()[index]["id"] | "", sizeof(p.message));
  strlcpy(p.name, name.c_str(), sizeof(p.name));
  uint8_t broadcast[6];
  memset(broadcast, 255, 6);
  lastSend = millis();
  if (esp_now_send(broadcast, reinterpret_cast<uint8_t *>(&p), sizeof(p)) != ESP_OK)
    return false;
  if (count == 8) {
    for (int i = 1; i < 8; i++)
      history[i - 1] = history[i];
    count--;
  }
  history[count++] = {"Ich (gesendet)", messages()[index]["text"].as<String>(), millis()};
  revision++;
  return true;
}
} // namespace leap
