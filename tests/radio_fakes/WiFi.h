#pragma once
#include <Arduino.h>
constexpr int WL_CONNECTED = 3;
inline bool radioDnsWorks = true;
inline int radioWifiStatus = WL_CONNECTED;
class IPAddress { public: String toString() const { return "192.0.2.10"; } };
struct FakeNetwork {
  int hostByName(const char *, IPAddress &) { radioNow += 25; return radioDnsWorks; }
};
inline FakeNetwork Network;
struct FakeWiFi { int status() { return radioWifiStatus; } int RSSI() { return -60; } };
inline FakeWiFi WiFi;
