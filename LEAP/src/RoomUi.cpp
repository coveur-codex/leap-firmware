#include "Ui.h"
#include "Network.h"
#include "AccentColor.h"
#include "DisplayText.h"
namespace leap {
void Ui::drawRoom() {
  const uint16_t accent=accentColor(state["config"]["accentColor"] | "");
  bool fresh=roomFresh(roomFrame,network.connected,millis());
  if(!roomFrame["lights"].is<JsonArray>()) {
    body(roomFrame["notice"] | (network.connected ? "Zimmer wird geladen…" : "Kein WLAN · Steuerung nicht erreichbar"));
    return;
  }
  bool hasHeater=roomFrame["climate"].is<JsonObject>();
  int rows=int(roomFrame["lights"].size())+(hasHeater?1:0);
  roomControls.selection=constrain(roomControls.selection,0,std::max(0,rows-1));
  for(int i=0;i<rows;++i) {
    int slot=i+(hasHeater?0:1), y=8+i*36;
    auto row=RoomControls::row(roomFrame,slot);
    auto &draft=roomControls.drafts[slot];
    if(i==roomControls.selection) canvas->drawRoundRect(92,y,332,34,4,accent);
    String name=row["name"] | (slot==0?"Heizung":"Lampe");
    glyphText(String(displayGlyphs(name.c_str()).substr(0,34).c_str()),98,y+4,1,i==roomControls.selection?accent:0xffff);
    String status;
    if(!fresh) status="Verbindung fehlt / Stand veraltet";
    else if(row["available"]!=true) status="Nicht verfügbar";
    else if(slot==0) {
      status="Ist "; status+=row["current"].is<double>()?String(row["current"].as<double>(),1)+"°":"?";
      status+="  Ziel ";
      status+=draft.active?String(draft.temperature,1)+"°C *":row["target"].is<double>()?String(row["target"].as<double>(),1)+"°C":"?";
      if(row["controllable"]!=true) status="Heizung nicht steuerbar";
      else if(roomFrame["temperatureSetSupported"]!=true) status="Bitte Homeserver aktualisieren";
    } else {
      int color=draft.active?draft.color:RoomControls::colorOf(row);
      uint32_t rgb=RoomPalette[color].rgb;
      uint16_t ink=((rgb>>19)<<11)|(((rgb>>10)&63)<<5)|((rgb>>3)&31);
      if(!fresh || row["available"]!=true) ink=0x4208;
      canvas->fillRoundRect(361,y+4,44,26,4,ink);
      canvas->drawRoundRect(361,y+4,44,26,4,0xffff);
      text("<",350,y+13,1,accent); text(">",411,y+13,1,accent);
      if(status.length()==0) {
        status=(!draft.active && row["on"]==true && row["color"].isNull())?"An":RoomPalette[color].label;
        if(draft.active) status+=" *";
      }
    }
    text(status,98,y+19);
  }
  String footer;
  if(roomControls.changed()) footer="Auswahl · Übernahme nach 1 s Pause";
  else if(network.roomPending) footer="Wird übertragen…";
  else if(roomControls.failed) footer="Nicht bestätigt · bitte erneut wählen";
  else if(roomControls.awaiting()) footer="Warte auf Bestätigung…";
  else footer="Hoch/runter: Gerät · L/R: Temperatur/Farbe";
  text(footer,94,124);
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
  roomControls.input(roomFrame,network.connected,millis(),key);
}
} // namespace leap
