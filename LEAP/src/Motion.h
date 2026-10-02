#pragma once
#include "Input.h"
namespace leap {
// Fixed MPU6050 hardware, no generic sensor framework. Main/UI task owns I2C.
class Motion {
  uint8_t address = 0;
  uint32_t sampled = 0, lastDirection = 0, lastShake = 0;
  bool write(uint8_t reg, uint8_t value);
  bool read(uint8_t reg, uint8_t *data, size_t size);

public:
  bool available = false;
  float x = 0, y = 0, z = 1;
  float gx = 0, gy = 0, gz = 0;
  bool begin();
  void poll();
  int direction(bool requireNeutral = false);
  bool shake();
};
extern Motion motion;
} // namespace leap
