#include "NetworkProfiles.h"
#include "WifiEditor.h"
#include <cassert>
#include <cstdio>
namespace leap { DeviceSettings deviceSettings; }
using namespace leap;
static WifiProfile profile(const char *ssid, const char *password="password123") {
  WifiProfile p; strcpy(p.ssid,ssid); strcpy(p.password,password); return p;
}
int main() {
  strcpy(deviceSettings.server,"http://192.168.1.10:8080");
  strcpy(deviceSettings.ssid,"Home"); strcpy(deviceSettings.password,"homepass");
  Preferences::bytes.clear();
  Preferences::namespaces.clear();
  Preferences unopened;
  assert(!unopened.begin("leap-network", true));
  assert(!Preferences::namespaces.count("leap-network"));
  // The original device configuration is the only namespace on a 1.2.0 device.
  strcpy(deviceSettings.deviceId,"leap-existing");
  strcpy(deviceSettings.timezone,"UTC0");
  deviceSettings.revision=1;
  deviceSettings.checksum=deviceSettings.hash();
  Preferences devicePrefs;
  DeviceSettings original=deviceSettings, loaded;
  assert(loadDeviceSettings(devicePrefs,loaded,&original)); devicePrefs.end();
  auto originalBytes=Preferences::bytes["leap-deviceconfig"];
  auto originalWrites=Preferences::writes["leap-deviceconfig"];
  Preferences::failWrites=true;
  NetworkProfiles failedFirstSave;
  assert(!failedFirstSave.begin());
  assert(Preferences::bytes["leap-deviceconfig"]==originalBytes);
  assert(Preferences::bytes["leap-networkprofiles"].empty());
  Preferences::failWrites=false;
  NetworkProfiles manager;
  assert(manager.begin());
  assert(Preferences::namespaces.count("leap-network"));
  assert(Preferences::bytes["leap-deviceconfig"]==originalBytes);
  assert(Preferences::writes["leap-deviceconfig"]==originalWrites);
  assert(loadDeviceSettings(devicePrefs,loaded)); devicePrefs.end();
  assert(loaded.valid() && !strcmp(loaded.deviceId,original.deviceId));
  assert(manager.endpoint()=="http://ndk.tstromi.de:8080");
  manager.nextEndpoint(); assert(manager.endpoint()==deviceSettings.server);
  WifiProfile p;
  assert(manager.profile(0,p) && !strcmp(p.ssid,"Home"));
  assert(manager.saveLocal(profile("Travel")));
  assert(manager.profile(0,p) && !strcmp(p.ssid,"Travel"));
  WifiProfile remote[4]{};remote[0]=profile("School");remote[1]=profile("Travel","otherpass");
  assert(manager.applyRemote(1,"http://example.com:8080",remote));
  assert(manager.profile(0,p) && !strcmp(p.password,"password123"));
  assert(manager.profile(1,p) && !strcmp(p.ssid,"School"));
  assert(manager.profile(2,p) && !strcmp(p.ssid,"Home"));
  assert(!manager.profile(3,p));
  auto writes=Preferences::writes["leap-networkprofiles"];
  assert(manager.applyRemote(1,"http://example.com:8080",remote));
  assert(Preferences::writes["leap-networkprofiles"]==writes);
  auto networkWrites=Preferences::writes["leap-networkprofiles"];
  NetworkProfiles reboot; assert(reboot.begin());
  assert(Preferences::writes["leap-networkprofiles"]==networkWrites);
  assert(reboot.profile(0,p) && !strcmp(p.ssid,"Travel"));
  assert(reboot.saveLocal(profile("Third"))); assert(reboot.saveLocal(profile("Fourth"))); assert(reboot.saveLocal(profile("Extra")));
  assert(!reboot.saveLocal(profile("Fifth")));
  Preferences::failWrites=true;
  assert(!reboot.saveLocal(profile("Travel","newpassword")));
  assert(reboot.profile(3,p) && !strcmp(p.ssid,"Travel") && !strcmp(p.password,"password123"));
  Preferences::failWrites=false;
  assert(reboot.removeLocal(0)); assert(reboot.saveLocal(profile("Fifth")));
  assert(!reboot.saveLocal(profile("Bad","short")));
  assert(!reboot.applyRemote(2,"ftp://bad",remote));
  Preferences::bytes["leap-networkprofiles"][0]=42;
  NetworkProfiles corrupt; assert(!corrupt.begin());
  WifiEditor editor;
  auto right=[](Key k) { return InputEvent{true,k,false}; };
  editor.input(right(Key::Center),0); assert(editor.field==WifiEditor::Field::Ssid);
  editor.character='A'; for (int i=0;i<40;++i) editor.input(right(Key::Up),0);
  assert(editor.ssid.size()==32);
  editor.input(right(Key::Down),0); assert(editor.ssid.size()==31);
  editor.input(right(Key::Center),0);
  editor.input(right(Key::Down),0); editor.input(right(Key::Center),0);
  editor.character=32;editor.input(right(Key::Up),0);assert(editor.password==" ");
  editor.input(right(Key::Left),0);assert(editor.character==133);
  editor.input(right(Key::Right),0);assert(editor.character==32);
  editor.character=127;editor.input(right(Key::Up),0);assert(editor.password==" ä");
  editor.input(right(Key::Down),0);assert(editor.password==" ");
  assert(editor.input({false,Key::Center,false},0)==WifiEditor::Action::None);
  assert(editor.field==WifiEditor::Field::Menu);
  assert(editor.input({false,Key::Center,false},0)==WifiEditor::Action::Close);
  puts("PASS: OTA network migration, NVS failure, reboot, WLAN sources/priority, limits and keyboard");
}
