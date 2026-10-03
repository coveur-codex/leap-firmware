#pragma once
#include <stdint.h>
namespace leap {
// PNGdec 1.1.6 expects 0x00BBGGRR, not the usual 0x00RRGGBB.
inline uint32_t pngBackground(uint16_t rgb565) {
  uint32_t r = (rgb565 >> 11) & 31, g = (rgb565 >> 5) & 63, b = rgb565 & 31;
  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);
  return r | (g << 8) | (b << 16);
}
} // namespace leap
