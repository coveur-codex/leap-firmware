#pragma once
#include "ReadingFont.h"
#include <algorithm>
#include <string>

namespace leap {
// The whole word (including visible hyphens) shares one face and baseline.
struct ReadingBounds { int left, width, advance; };
inline ReadingBounds readingBounds(const std::string &glyphs, const GFXfont &font) {
  int cursor = 0, left = 0, right = 0;
  for (unsigned char c : glyphs) {
    if (c < font.first || c > font.last) continue;
    const auto &glyph = font.glyph[c - font.first];
    left = std::min(left, cursor + glyph.xOffset);
    right = std::max(right, cursor + glyph.xOffset + glyph.width);
    cursor += glyph.xAdvance;
    right = std::max(right, cursor);
  }
  return {left, right - left, cursor};
}
inline int readingAdvance(const std::string &glyphs, const GFXfont &font) {
  return readingBounds(glyphs, font).advance;
}
inline int readingFace(const std::string &glyphs, int width = 326) {
  for (int face = 4; face > 0; --face)
    if (readingBounds(glyphs, ReadingFonts[face]).width <= width) return face;
  return 0;
}
template <typename Graphics>
void printReadingPart(Graphics &gfx, const std::string &glyphs, int face, int x,
                      int baseline, uint16_t color) {
  gfx.setFont(&ReadingFonts[face]);
  gfx.setTextSize(1); // Every curve is rasterized at its actual pixel size.
  gfx.setTextColor(color);
  gfx.setCursor(x, baseline);
  gfx.print(glyphs.c_str());
  gfx.setFont(); // Menus, hints and the next page retain the existing UI font.
}
} // namespace leap
