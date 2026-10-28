#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace leap {
enum class RoomKey { Up, Down, Left, Right, Center };
struct RoomColor { const char *id, *label; uint32_t rgb; };
inline constexpr RoomColor RoomPalette[] = {
  {"aus", "Aus", 0x000000}, {"weiss", "Weiß", 0xffebc8},
  {"gelb", "Gelb", 0xffc800}, {"orange", "Orange", 0xff6400},
  {"rot", "Rot", 0xff0000}, {"rosa", "Rosa", 0xff50a0},
  {"lila", "Lila", 0xb400ff}, {"blau", "Blau", 0x0000ff},
  {"hellblau", "Hellblau", 0x50b4ff}, {"tuerkis", "Türkis", 0x00ffc8},
  {"gruen", "Grün", 0x00ff00}};
inline constexpr int RoomColorCount = sizeof(RoomPalette)/sizeof(RoomPalette[0]);
struct RoomCommand {
  const char *target=nullptr, *action=nullptr, *color=nullptr;
  double temperature=0;
  int slot=-1;
  uint32_t ticket=0;
};
inline bool roomFresh(JsonVariantConst frame, bool online, uint32_t now) {
  return online && frame["receivedMillis"].is<uint32_t>() &&
         uint32_t(now-frame["receivedMillis"].as<uint32_t>()) < 10000;
}
struct RoomDraft {
  bool active=false, dirty=false, awaiting=false, force=false;
  double temperature=0;
  int color=0;
  uint32_t revision=0, sentAt=0;
};
struct RoomControls {
  int selection=0, inFlight=-1;
  RoomDraft drafts[3]; // Stable targets: climate, light0, light1, independent of visible rows.
  uint32_t lastChange=0, ticket=0, sentRevision=0;
  bool failed=false;
  static JsonVariantConst row(JsonVariantConst frame, int slot) {
    return slot==0 ? frame["climate"] : frame["lights"][slot-1];
  }
  static int colorOf(JsonVariantConst lamp) {
    if (lamp["on"] != true) return 0;
    const char *id=lamp["color"] | "";
    for (int i=1;i<RoomColorCount;++i) if (!strcmp(id,RoomPalette[i].id)) return i;
    return 1; // An externally chosen color without RGB metadata is shown as "An".
  }
  bool changed() const { for (const auto &d:drafts) if(d.dirty) return true; return false; }
  bool awaiting() const { for (const auto &d:drafts) if(d.active && d.awaiting) return true; return false; }
  void cancel() { for(auto &d:drafts) d={}; inFlight=-1; }
  static bool matches(JsonVariantConst actual, const RoomDraft &d, int slot) {
    if (actual["available"]!=true) return false;
    if (!slot) return actual["target"].is<double>() && std::abs(actual["target"].as<double>()-d.temperature)<.01;
    if (!d.color) return actual["on"]==false;
    return actual["on"]==true && (actual["colorSupported"]!=true || actual["color"]==RoomPalette[d.color].id);
  }
  void observe(JsonVariantConst frame, bool online, uint32_t now) {
    if (!roomFresh(frame,online,now)) { if(changed()) failed=true; cancel(); return; }
    for(int i=0;i<3;++i) {
      auto &d=drafts[i];
      if(!d.active) continue;
      auto actual=row(frame,i);
      bool usable=actual["available"]==true && (i || actual["controllable"]==true);
      if(!usable || (!i && (d.temperature<(actual["minimum"]|10.0) || d.temperature>(actual["maximum"]|30.0)))) {
        d={}; failed=true; continue;
      }
      if(inFlight==i) continue;
      if(!d.force && matches(actual,d,i)) { d={}; continue; }
      if(!d.dirty && d.awaiting && uint32_t(now-d.sentAt)>=15000) { d={}; failed=true; }
    }
  }
  void input(JsonVariantConst frame, bool online, uint32_t now, RoomKey key) {
    observe(frame,online,now);
    if(!roomFresh(frame,online,now)) return;
    bool hasHeater=frame["climate"].is<JsonObjectConst>();
    int rows=int(frame["lights"].size())+(hasHeater?1:0);
    if(!rows || frame["lights"].size()>2) return;
    selection=std::clamp(selection,0,rows-1);
    int direction=(key==RoomKey::Down)-(key==RoomKey::Up);
    if(direction) { selection=std::clamp(selection+direction,0,rows-1); return; }
    int delta=(key==RoomKey::Right)-(key==RoomKey::Left);
    if(!delta) return;
    int slot=selection+(hasHeater?0:1);
    auto actual=row(frame,slot);
    if(actual["available"]!=true) return;
    auto &d=drafts[slot];
    if(!slot) {
      if(actual["controllable"]!=true || frame["temperatureSetSupported"]!=true || !actual["target"].is<double>()) return;
      double base=d.active?d.temperature:actual["target"].as<double>();
      double target=base+delta;
      if(target<(actual["minimum"]|10.0) || target>(actual["maximum"]|30.0)) return;
      d.temperature=target;
    } else {
      int base=d.active?d.color:colorOf(actual);
      int count=actual["colorSupported"]==true?RoomColorCount:2;
      int choice=(base+count+delta)%count;
      // Old servers may expose only a subset of the new palette.
      for(int tries=0;tries<count;++tries) {
        bool supported=choice<2 || frame["colors"].isNull();
        for(JsonVariantConst id:frame["colors"].as<JsonArrayConst>()) supported|=id==RoomPalette[choice].id;
        if(supported) break;
        choice=(choice+count+delta)%count;
      }
      d.color=choice;
    }
    d.force|=inFlight==slot || d.awaiting;
    d.active=d.dirty=true; d.awaiting=false; ++d.revision;
    lastChange=now; failed=false;
    if(!d.force && inFlight!=slot && matches(actual,d,slot)) d={};
  }
  RoomCommand next(JsonVariantConst frame, bool online, uint32_t now) const {
    if(inFlight>=0 || !roomFresh(frame,online,now) || uint32_t(now-lastChange)<1000) return {};
    for(int i=0;i<3;++i) if(drafts[i].dirty) {
      RoomCommand cmd;
      cmd.slot=i; cmd.ticket=ticket+1; if(!cmd.ticket) ++cmd.ticket;
      cmd.target=i==0?"climate":i==1?"light0":"light1";
      if(!i) { cmd.action="set_temperature"; cmd.temperature=drafts[i].temperature; }
      else { cmd.action=drafts[i].color ? (row(frame,i)["colorSupported"]==true?"color":"on") : "off";
        if(drafts[i].color && row(frame,i)["colorSupported"]==true) cmd.color=RoomPalette[drafts[i].color].id; }
      return cmd;
    }
    return {};
  }
  void submitted(const RoomCommand &cmd, uint32_t now) {
    inFlight=cmd.slot; ticket=cmd.ticket; sentRevision=drafts[cmd.slot].revision;
    drafts[cmd.slot].dirty=false; drafts[cmd.slot].sentAt=now;
  }
  void complete(uint32_t completed, bool success, uint32_t now) {
    if(inFlight<0 || completed!=ticket) return;
    auto &d=drafts[inFlight];
    if(!success) { d={}; failed=true; }
    else if(d.revision==sentRevision) { d.awaiting=true; d.force=false; d.sentAt=now; }
    inFlight=-1;
  }
};
} // namespace leap
