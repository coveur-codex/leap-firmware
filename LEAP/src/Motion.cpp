#include "Motion.h"
#include "Config.h"
#include "Hardware.h"
#include <Wire.h>
#include <math.h>
namespace leap {
Motion motion;
bool Motion::write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
bool Motion::read(uint8_t reg, uint8_t *bytes, size_t size) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom(address, uint8_t(size)) != size)
    return false;
  for (size_t i = 0; i < size; i++)
    bytes[i] = Wire.read();
  return true;
}
bool Motion::begin() {
  Wire.begin(hw::ImuSda, hw::ImuScl, 400000);
  Wire.setTimeOut(5);
  for (uint8_t candidate : {uint8_t(0x68), uint8_t(0x69)}) {
    address = candidate;
    uint8_t who = 0;
    if (read(0x75, &who, 1) && who == 0x68) {
      available = write(0x6b, 0x01) && write(0x1a, 0x03) && write(0x19, 0x04) && write(0x1b, 0) &&
                  write(0x1c, 0);
      if (available) {
        log("IMU", "MPU6050 ready: +/-2g, +/-250deg/s, DLPF 44Hz");
        return true;
      }
    }
  }
  address = 0;
  available = false;
  log("IMU", "MPU6050 unavailable; switch controls remain usable");
  return false;
}
void Motion::poll() {
  if (!available || !elapsed(millis(), sampled, 20))
    return;
  sampled = millis();
  uint8_t bytes[14];
  if (!read(0x3b, bytes, sizeof(bytes))) {
    available = false;
    log("IMU", "I2C read failed; using switches");
    return;
  }
  auto signed16 = [&](int i) { return int16_t((uint16_t(bytes[i]) << 8) | bytes[i + 1]); };
  float ax = signed16(0) / 16384.0f, ay = signed16(2) / 16384.0f;
  x = ax;
  y = ay;
  z = signed16(4) / 16384.0f;
  gx = signed16(8) / 131.0f;
  gy = signed16(10) / 131.0f;
  gz = signed16(12) / 131.0f;
  deviceSettings.orient(x, y, z);
  deviceSettings.orient(gx, gy, gz);
}
int Motion::direction(bool requireNeutral) {
  static bool armed = true;
  if (!available)
    return -1;
  if (fabsf(x) < 0.20f && fabsf(y) < 0.20f) {
    armed = true;
    return -1;
  }
  if (!elapsed(millis(), lastDirection, 220) || (requireNeutral && !armed))
    return -1;
  int key = -1;
  if (fabsf(x) > fabsf(y) && fabsf(x) > 0.38f)
    key = int(x > 0 ? Key::Right : Key::Left);
  else if (fabsf(y) > 0.38f)
    key = int(y > 0 ? Key::Down : Key::Up);
  if (key >= 0) {
    lastDirection = millis();
    armed = false;
  }
  return key;
}
bool Motion::shake() {
  if (!available || !elapsed(millis(), lastShake, 500))
    return false;
  if (x * x + y * y + z * z > 3.3f) {
    lastShake = millis();
    return true;
  }
  return false;
}
} // namespace leap
