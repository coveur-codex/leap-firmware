#include "Ui.h"
#include "Network.h"
#include "AccentColor.h"
namespace leap {
void Ui::drawRoom() {
  const uint16_t accent = accentColor(state["config"]["accentColor"] | "");
  bool fresh = roomFresh(roomFrame,network.connected,millis());
  if (!roomFrame["lights"].is<JsonArray>()) {
    body(roomFrame["notice"] | (network.connected ? "Zimmer wird geladen…" : "Kein WLAN · Steuerung nicht erreichbar"));
    return;
  }
  int rows = int(roomFrame["lights"].size()) + (roomFrame["climate"].is<JsonObject>() ? 1 : 0);
  roomControls.selection = constrain(roomControls.selection, 0, std::max(0, rows-1));
  for (int i=0; i<rows; ++i) {
    bool heater = i == 0 && roomFrame["climate"].is<JsonObject>();
    int index = i - (roomFrame["climate"].is<JsonObject>() ? 1 : 0);
    JsonVariant row = heater ? roomFrame["climate"].as<JsonVariant>() : roomFrame["lights"][index].as<JsonVariant>();
    int y=8+i*36;
    if (i == roomControls.selection) canvas->drawRoundRect(92,y,332,34,4,accent);
    String name = row["name"] | (heater ? "Heizung" : "Lampe");
    text(name.substring(0,38),98,y+4,1,i==roomControls.selection ? accent : 0xffff);
    String status;
    if (!fresh) status="Verbindung fehlt / Stand veraltet";
    else if (row["available"] != true) status="Nicht verfügbar";
    else if (heater) {
      status="Ist "; status += row["current"].is<float>() ? String(row["current"].as<float>(),1)+"°" : "?";
      status+="  Ziel "; status += row["target"].is<float>() ? String(row["target"].as<float>(),1)+"°C" : "?";
      if (row["controllable"] != true) status="Heizung nicht steuerbar";
    } else status = row["on"] == true ? "An" : "Aus";
    text(status,98,y+19);
  }
  String footer;
  if (roomControls.editing) footer="Farbe: "+String(RoomColorNames[roomControls.color])+"  L/R wählen · Mitte senden";
  else if (network.roomPending) footer="Befehl wird gesendet…";
  else if (roomFrame["notice"].is<const char *>()) footer=roomFrame["notice"].as<String>();
  else footer="Hoch/runter: Gerät · L/R: −/+ oder aus/an";
  text(footer.substring(0,54),94,124);
}
void Ui::roomAction(const InputEvent &e) {
  RoomKey key;
  switch(e.key) {
    case Key::Up: key=RoomKey::Up; break;
    case Key::Down: key=RoomKey::Down; break;
    case Key::Left: key=RoomKey::Left; break;
    case Key::Right: key=RoomKey::Right; break;
    default: key=RoomKey::Center; break;
  }
  auto cmd=roomControls.input(roomFrame,network.connected,millis(),key);
  if (cmd.action && !network.roomCommand(cmd.target,cmd.action,cmd.color))
    roomFrame["notice"]="Bitte warten · Befehl läuft oder kein WLAN";
}
} // namespace leap
