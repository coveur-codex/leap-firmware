#pragma once
constexpr int WL_CONNECTED = 3;
class WiFiClient {};
struct FakeWifi { int status() const { return WL_CONNECTED; } };
inline FakeWifi WiFi;
