#pragma once
#define LEAP_WIFI_SSID ""
#define LEAP_WIFI_PASSWORD ""
#define LEAP_SERVER "http://192.168.1.10:8000"
#define LEAP_DEVICE_ID "leap-lars"
// POSIX TZ; no network dependency. Homeserver currently has no timezone field.
#define LEAP_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define LEAP_RADIO_CHANNEL 6
// All nearby devices must share a channel, including the WLAN access point.
// Pinned core 3.3.0 supplies a rollback-enabled bootloader.
// Initial USB upload must include that bootloader; acceptance-test rollback.
#define LEAP_OTA_ENABLED 1
// Optional PEM root CA for an https:// server; never uses setInsecure().
#define LEAP_TLS_CA ""
