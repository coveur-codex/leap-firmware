#pragma once
#include <PNGdec.h>
namespace leap {
// Binary sprite transparency. Read only this pixel; PNGdec's packed-mask helper
// rounds up to groups of eight and does not support one-bit palette alpha.
inline bool pngOpaque(const PNGDRAW &row, int x, uint32_t transparent) {
  if (!row.iHasAlpha)
    return true;
  if (row.iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA)
    return row.pPixels[x * 4 + 3] >= 128;
  if (row.iPixelType == PNG_PIXEL_GRAY_ALPHA)
    return row.pPixels[x * 2 + 1] >= 128;
  if (row.iPixelType == PNG_PIXEL_TRUECOLOR) {
    auto p = row.pPixels + x * 3;
    return (uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]) != transparent;
  }
  if (row.iPixelType == PNG_PIXEL_INDEXED || row.iPixelType == PNG_PIXEL_GRAYSCALE) {
    if (row.iBpp < 1 || row.iBpp > 8)
      return true;
    int perByte = 8 / row.iBpp;
    int index =
        (row.pPixels[x / perByte] >> (8 - row.iBpp * (x % perByte + 1))) & ((1 << row.iBpp) - 1);
    return row.iPixelType == PNG_PIXEL_INDEXED ? row.pPalette[768 + index] >= 128
                                               : uint32_t(index) != transparent;
  }
  return true;
}
} // namespace leap
