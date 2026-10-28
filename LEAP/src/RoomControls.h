#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdint>
namespace leap {
enum class RoomKey { Up, Down, Left, Right, Center };
struct RoomCommand { const char *target=nullptr, *action=nullptr, *color=nullptr; };
inline constexpr const char *RoomColors[] = {"weiss", "rot", "gruen", "blau", "lila", "gelb"};
inline constexpr const char *RoomColorNames[] = {"Weiß", "Rot", "Grün", "Blau", "Lila", "Gelb"};
inline bool roomFresh(JsonVariantConst frame, bool online, uint32_t now) {
  return online && frame["receivedMillis"].is<uint32_t>() &&
         uint32_t(now-frame["receivedMillis"].as<uint32_t>()) < 10000;
}
struct RoomControls {
  int selection=0, color=0;
  bool editing=false;
  RoomCommand input(JsonVariantConst frame, bool online, uint32_t now, RoomKey key) {
    if (!roomFresh(frame,online,now)) return {};
    bool hasHeater=frame["climate"].is<JsonObjectConst>();
    int rows=int(frame["lights"].size())+(hasHeater ? 1 : 0);
    if (!rows || frame["lights"].size()>2) return {};
    selection=std::clamp(selection,0,rows-1);
    int direction=(key==RoomKey::Down)-(key==RoomKey::Up);
    if (direction) { selection=std::clamp(selection+direction,0,rows-1); editing=false; return {}; }
    bool heater=hasHeater && selection==0;
    int index=selection-(hasHeater ? 1 : 0);
    JsonVariantConst row=heater ? frame["climate"] : frame["lights"][index];
    if (row["available"] != true) return {};
    RoomCommand command;
    command.target=heater ? "climate" : (index==0 ? "light0" : "light1");
    if (editing) {
      if (heater || row["colorSupported"]!=true) { editing=false; return {}; }
      if (key==RoomKey::Left) color=(color+5)%6;
      if (key==RoomKey::Right) color=(color+1)%6;
      if (key==RoomKey::Center) { command.action="color"; command.color=RoomColors[color]; editing=false; }
    } else if (heater) {
      if (row["controllable"] != true) return {};
      if (key==RoomKey::Left) command.action="colder";
      if (key==RoomKey::Right) command.action="warmer";
      if (command.action && row["target"].is<double>() && row["minimum"].is<double>() && row["maximum"].is<double>()) {
        double target=row["target"].as<double>()+(key==RoomKey::Right ? 1 : -1);
        if (target<row["minimum"].as<double>() || target>row["maximum"].as<double>()) command.action=nullptr;
      }
    } else {
      if (key==RoomKey::Left) command.action="off";
      if (key==RoomKey::Right) command.action="on";
      if (key==RoomKey::Center && row["colorSupported"]==true) editing=true;
    }
    return command;
  }
};
} // namespace leap
