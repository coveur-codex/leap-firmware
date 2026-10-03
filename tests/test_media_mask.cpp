#include "MediaMask.h"
#include <cassert>
int main() {
  PNGDRAW row{};
  uint8_t pixels[] = {0x80, 0x20};
  uint8_t palette[1024]{};
  palette[769] = 255;
  row.pPixels = pixels;
  row.pPalette = palette;
  row.iPixelType = PNG_PIXEL_INDEXED;
  row.iBpp = 1;
  row.iHasAlpha = 1;
  assert(leap::pngOpaque(row, 0, 0));
  assert(!leap::pngOpaque(row, 1, 0));
  assert(!leap::pngOpaque(row, 8, 0));
  assert(leap::pngOpaque(row, 10, 0));
  row.iHasAlpha = 0;
  assert(leap::pngOpaque(row, 1, 0));
  uint8_t rgb[] = {0, 0, 0, 255, 0, 0};
  row.pPixels = rgb;
  row.iHasAlpha = 1;
  row.iPixelType = PNG_PIXEL_TRUECOLOR;
  assert(!leap::pngOpaque(row, 0, 0));
  assert(leap::pngOpaque(row, 1, 0));
  assert(!leap::pngOpaque(row, 1, 0xff0000));
  row.iPixelType = PNG_PIXEL_GRAYSCALE;
  row.iBpp = 8;
  assert(!leap::pngOpaque(row, 0, 0));
  assert(leap::pngOpaque(row, 3, 0));
}
