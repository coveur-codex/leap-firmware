#include "RoomControls.h"
#include <cassert>
#include <cstring>
using namespace leap;
static bool is(const char *a,const char *b) { return a && !strcmp(a,b); }
int main() {
  JsonDocument frame;
  frame["receivedMillis"]=uint32_t(100);
  auto heater=frame["climate"].to<JsonObject>(); heater["available"]=true; heater["controllable"]=true;
  auto lights=frame["lights"].to<JsonArray>();
  for(int i=0;i<2;++i) { auto lamp=lights.add<JsonObject>(); lamp["available"]=true; lamp["colorSupported"]=i==0; }
  RoomControls nav;
  auto press=[&](RoomKey key) {return nav.input(frame,true,100,key);};
  assert(is(press(RoomKey::Right).action,"warmer"));
  assert(is(press(RoomKey::Left).action,"colder"));
  assert(!press(RoomKey::Center).action);
  press(RoomKey::Up); assert(nav.selection==0);
  press(RoomKey::Down);
  auto on=press(RoomKey::Right); assert(is(on.target,"light0") && is(on.action,"on"));
  assert(is(press(RoomKey::Left).action,"off"));
  press(RoomKey::Center); assert(nav.editing);
  press(RoomKey::Left); assert(nav.color==5);
  auto color=press(RoomKey::Center); assert(is(color.action,"color") && is(color.color,"gelb") && !nav.editing);
  press(RoomKey::Center); press(RoomKey::Down); assert(!nav.editing && nav.selection==2);
  press(RoomKey::Down); assert(nav.selection==2);
  press(RoomKey::Center); assert(!nav.editing);
  assert(is(press(RoomKey::Right).target,"light1"));
  frame["lights"][1]["available"]=false; assert(!press(RoomKey::Right).action);
  nav.selection=0; heater["controllable"]=false; assert(!press(RoomKey::Right).action);
  heater["controllable"]=true;
  assert(!nav.input(frame,false,100,RoomKey::Right).action);
  assert(!nav.input(frame,true,10100,RoomKey::Right).action);
  frame["receivedMillis"]=uint32_t(UINT32_MAX-50);
  assert(nav.input(frame,true,20,RoomKey::Right).action); // millis rollover
  heater["target"]=30; heater["minimum"]=10; heater["maximum"]=30;
  frame["receivedMillis"]=uint32_t(100);
  assert(!press(RoomKey::Right).action); assert(press(RoomKey::Left).action);
  heater["target"]=10; assert(!press(RoomKey::Left).action);
  frame.remove("climate"); frame["receivedMillis"]=uint32_t(100);
  assert(is(press(RoomKey::Right).target,"light0"));
  frame["lights"].clear(); assert(!press(RoomKey::Right).action);
}
