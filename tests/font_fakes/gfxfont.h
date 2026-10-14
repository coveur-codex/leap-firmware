#pragma once
#include <cstdint>
// Native fallback for Arduino_GFX's font data ABI. CI also uses the real 1.6.3 header.
struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t *bitmap;
  GFXglyph *glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};
