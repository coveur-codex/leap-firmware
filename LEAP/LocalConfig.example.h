#pragma once
// Cable installation: persist these values once. For changes increment revision.
// Universal OTA: build without LocalConfig.h (or set provisioning to 0).
#ifndef LEAP_PROVISION_DEVICE
#define LEAP_PROVISION_DEVICE 1
#endif
#define LEAP_CONFIG_REVISION 1
#define LEAP_LEFT_KEYS {4, 5, 6, 7, 8}
#define LEAP_RIGHT_KEYS {2, 15, 16, 21, 47}
// Each array: Up, Down, Left, Right, Center as viewed by the user.
#define LEAP_IMU_SWAP_XY 0
#define LEAP_IMU_X_SIGN 1
#define LEAP_IMU_Y_SIGN 1
#define LEAP_IMU_Z_SIGN 1
// Z-down example (flipped around X): X=+1, Y=-1, Z=-1.
// Swap is applied before signs. The combination must be a physical rotation.
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
