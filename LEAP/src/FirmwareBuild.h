#pragma once
// One source for the boot banner, sync reports and embedded image identity.
#define LEAP_FIRMWARE_VERSION "1.5.1"
namespace leap {
inline constexpr char FirmwareVersion[] = LEAP_FIRMWARE_VERSION;
inline constexpr char FirmwareImageIdentity[] = "LEAP_APP_VERSION_V1=" LEAP_FIRMWARE_VERSION ";";
} // namespace leap
