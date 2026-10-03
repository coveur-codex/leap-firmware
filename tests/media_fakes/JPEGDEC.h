#pragma once
#include <cstdint>
struct JPEGFILE {};
struct JPEGDRAW {
  int x, y, iHeight, iWidthUsed, iWidth;
  uint16_t *pPixels;
};
constexpr int RGB565_LITTLE_ENDIAN = 0;
class JPEGDEC {
public:
  template <class... Args> bool open(Args...) {
    return false;
  } // PNG rendering test only.
  int getWidth() {
    return 0;
  }
  int getHeight() {
    return 0;
  }
  void setPixelType(int) {}
  bool decode(int, int, int) {
    return false;
  }
  void close() {}
};
