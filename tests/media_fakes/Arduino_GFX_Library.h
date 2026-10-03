#pragma once
#include <cstdint>
#include <vector>
struct Arduino_GFX {
  static constexpr int Width = 428, Height = 142;
  std::vector<uint16_t> pixels = std::vector<uint16_t>(Width * Height, 0);
  void drawPixel(int x, int y, uint16_t color) {
    pixels[y * Width + x] = color;
  }
  void draw16bitRGBBitmap(int x, int y, uint16_t *source, int w, int h) {
    for (int row = 0; row < h; ++row)
      for (int col = 0; col < w; ++col)
        drawPixel(x + col, y + row, source[row * w + col]);
  }
};
