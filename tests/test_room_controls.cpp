#include "RoomControls.h"
#include <cassert>
#include <cstring>
using namespace leap;
static bool is(const char *a,const char *b) { return a && !strcmp(a,b); }
static JsonDocument room(uint32_t now=100) {
  JsonDocument frame;
  frame["receivedMillis"]=now; frame["temperatureSetSupported"]=true;
  auto h=frame["climate"].to<JsonObject>();
  h["available"]=h["controllable"]=true; h["target"]=20.0; h["minimum"]=10; h["maximum"]=30;
  auto lights=frame["lights"].to<JsonArray>();
  for(int i=0;i<2;++i) { auto l=lights.add<JsonObject>(); l["available"]=true; l["on"]=false; l["color"]="aus"; l["colorSupported"]=i==0; }
  return frame;
}
int main() {
  auto frame=room();
  RoomControls nav;
  nav.input(frame,true,100,RoomKey::Right);
  nav.input(frame,true,250,RoomKey::Right);
  nav.input(frame,true,400,RoomKey::Right);
  nav.input(frame,true,550,RoomKey::Right);
  assert(nav.drafts[0].temperature==24 && nav.changed());
  assert(!nav.next(frame,true,1549).action);
  auto cmd=nav.next(frame,true,1550);
  assert(is(cmd.action,"set_temperature") && cmd.temperature==24);
  nav.submitted(cmd,1550);
  assert(!nav.next(frame,true,3000).action); // One request at a time.
  // Repeated incoming polls must not replace the preview or add relative steps.
  frame["climate"]["target"]=21;
  nav.observe(frame,true,1600); assert(nav.drafts[0].temperature==24);
  nav.input(frame,true,1700,RoomKey::Right); assert(nav.drafts[0].temperature==25);
  nav.complete(cmd.ticket,true,1800); assert(nav.changed());
  assert(!nav.next(frame,true,2699).action);
  auto next=nav.next(frame,true,2700); assert(next.temperature==25);
  nav.submitted(next,2700); nav.complete(next.ticket,true,2800);
  assert(nav.awaiting() && !nav.changed());
  nav.observe(frame,true,2900); assert(nav.drafts[0].temperature==25 && nav.awaiting());
  frame["climate"]["target"]=25;
  nav.observe(frame,true,3000); assert(!nav.drafts[0].active);
  frame=room(); nav=RoomControls{};
  // Undoing an unsent choice needs no network call.
  nav.input(frame,true,100,RoomKey::Right); nav.input(frame,true,200,RoomKey::Left);
  assert(!nav.changed() && !nav.next(frame,true,1200).action);
  frame["climate"]["target"]=30; nav.input(frame,true,100,RoomKey::Right); assert(!nav.changed());
  frame["climate"]["target"]=10; nav.input(frame,true,100,RoomKey::Left); assert(!nav.changed());
  frame["climate"]["target"]=20.5; nav.input(frame,true,100,RoomKey::Left); assert(nav.drafts[0].temperature==19.5);
  frame=room(); nav=RoomControls{};
  nav.input(frame,true,100,RoomKey::Right);
  nav.input(frame,true,200,RoomKey::Down); nav.input(frame,true,500,RoomKey::Right);
  assert(!nav.next(frame,true,1499).action); // Global inactivity includes other lamps.
  cmd=nav.next(frame,true,1500); assert(cmd.slot==0);
  nav.submitted(cmd,1500); nav.complete(cmd.ticket,true,1550);
  cmd=nav.next(frame,true,1550); assert(cmd.slot==1 && is(cmd.color,"weiss"));
  nav.submitted(cmd,1550); nav.complete(cmd.ticket,true,1600);
  assert(!nav.next(frame,true,4000).action); // Accepted commands are never retried.
  // Direct palette selection, all ten colors, black = off, no Center confirmation.
  frame=room(); nav=RoomControls{}; nav.selection=1;
  for(int color=1;color<RoomColorCount;++color) {
    nav.input(frame,true,100+color*10,RoomKey::Right);
    assert(nav.drafts[1].color==color);
    cmd=nav.next(frame,true,1200+color*10);
    assert(is(cmd.action,"color") && is(cmd.color,RoomPalette[color].id));
  }
  nav.input(frame,true,300,RoomKey::Right); assert(!nav.changed()); // Back to original off.
  frame["lights"][0]["on"]=true; frame["lights"][0]["color"]="weiss";
  nav.input(frame,true,400,RoomKey::Left);
  cmd=nav.next(frame,true,1400); assert(is(cmd.action,"off"));
  // Different lamps keep separate local selections.
  nav.input(frame,true,500,RoomKey::Down); nav.input(frame,true,600,RoomKey::Right);
  cmd=nav.next(frame,true,1600); assert(cmd.slot==1);
  nav.submitted(cmd,1600); nav.complete(cmd.ticket,true,1700);
  cmd=nav.next(frame,true,1700); assert(cmd.slot==2 && is(cmd.action,"on"));
  // Network failure cancels newer edits on the same target rather than resending.
  frame=room(); nav=RoomControls{};
  nav.input(frame,true,100,RoomKey::Right); cmd=nav.next(frame,true,1100);
  nav.submitted(cmd,1100); nav.input(frame,true,1200,RoomKey::Right);
  nav.complete(cmd.ticket,false,1300); assert(nav.failed && !nav.changed());
  assert(!nav.next(frame,true,3000).action);
  nav.input(frame,true,1400,RoomKey::Right);
  nav.observe(frame,false,1500); assert(!nav.changed());
  nav.observe(frame,true,1600); assert(!nav.next(frame,true,3000).action);
  nav.input(frame,true,1700,RoomKey::Right);
  nav.observe(frame,true,10100); assert(!nav.changed());
  // Eventual confirmation timeout drops the preview without retrying.
  frame=room(); nav=RoomControls{};
  nav.input(frame,true,100,RoomKey::Right); cmd=nav.next(frame,true,1100);
  nav.submitted(cmd,1100); nav.complete(cmd.ticket,true,1200);
  frame["receivedMillis"]=uint32_t(16000); nav.observe(frame,true,16200);
  assert(nav.failed && !nav.drafts[0].active && !nav.next(frame,true,16200).action);
  // Returning to the old observed value during an unconfirmed write must send
  // a replacement, not accidentally cancel against a delayed status snapshot.
  frame=room(); nav=RoomControls{};
  nav.input(frame,true,100,RoomKey::Right); cmd=nav.next(frame,true,1100);
  nav.submitted(cmd,1100); nav.input(frame,true,1200,RoomKey::Left);
  nav.complete(cmd.ticket,true,1300); nav.observe(frame,true,1400);
  assert(nav.changed() && nav.next(frame,true,2200).temperature==20);
  nav=RoomControls{};
  nav.input(frame,true,100,RoomKey::Right); cmd=nav.next(frame,true,1100);
  nav.submitted(cmd,1100); nav.complete(cmd.ticket,true,1200);
  nav.input(frame,true,1300,RoomKey::Left); nav.observe(frame,true,1400);
  assert(nav.changed() && nav.next(frame,true,2300).temperature==20);
  // Millisecond rollover must preserve the exact one-second debounce.
  uint32_t start=UINT32_MAX-500;
  frame=room(start); nav=RoomControls{};
  nav.input(frame,true,start,RoomKey::Right);
  assert(!nav.next(frame,true,498).action); assert(nav.next(frame,true,499).action);
  // Disabled/old-server heaters and unavailable lamps cannot be edited.
  frame=room(); nav=RoomControls{};
  frame["temperatureSetSupported"]=false; nav.input(frame,true,100,RoomKey::Right); assert(!nav.changed());
  nav.selection=1; frame["lights"][0]["available"]=false;
  nav.input(frame,true,100,RoomKey::Right); assert(!nav.changed());
  frame.remove("climate"); nav.selection=0; frame["lights"][0]["available"]=true;
  nav.input(frame,true,100,RoomKey::Left);
  assert(nav.next(frame,true,1100).slot==1); // No-thermostat room retains stable targets.
  // A server exposing the old palette must not receive the four new names.
  frame=room(); auto palette=frame["colors"].to<JsonArray>(); palette.add("weiss"); palette.add("rot");
  nav=RoomControls{}; nav.selection=1;
  nav.input(frame,true,100,RoomKey::Right); nav.input(frame,true,200,RoomKey::Right);
  assert(is(nav.next(frame,true,1200).color,"rot"));
}
