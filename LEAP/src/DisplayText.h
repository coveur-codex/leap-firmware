#pragma once
#include <cstdint>
#include <string>

namespace leap {
// Arduino_GFX 1.6.3's built-in 6x8 font uses CP437 glyph indices, not UTF-8.
// Convert only at the display boundary. Each output byte occupies one cell,
// so wrapping, clipping and the enlarged article headings use the same glyphs.
inline std::string displayGlyphs(const char *utf8) {
  std::string out;
  const auto *p = reinterpret_cast<const unsigned char *>(utf8);
  while (*p) {
    uint32_t code = *p++;
    if (code >= 0x80) {
      unsigned remaining;
      uint32_t minimum;
      if (code >= 0xc2 && code <= 0xdf) {
        remaining = 1; minimum = 0x80; code &= 0x1f;
      } else if (code >= 0xe0 && code <= 0xef) {
        remaining = 2; minimum = 0x800; code &= 0x0f;
      } else if (code >= 0xf0 && code <= 0xf4) {
        remaining = 3; minimum = 0x10000; code &= 0x07;
      } else {
        out += '?';
        continue;
      }
      unsigned read = 0;
      while (read < remaining && (*p & 0xc0) == 0x80) {
        code = (code << 6) | (*p++ & 0x3f);
        ++read;
      }
      if (read != remaining || code < minimum || code > 0x10ffff ||
          (code >= 0xd800 && code <= 0xdfff)) {
        out += '?';
        continue;
      }
    }
    if (code < 0x80) {
      if (code == '\n' || code >= 32) out += char(code);
      continue;
    }
    switch (code) {
    case 0x00e4: out += char(0x84); break; // ä
    case 0x00f6: out += char(0x94); break; // ö
    case 0x00fc: out += char(0x81); break; // ü
    case 0x00c4: out += char(0x8e); break; // Ä
    case 0x00d6: out += char(0x99); break; // Ö
    case 0x00dc: out += char(0x9a); break; // Ü
    case 0x00df: out += char(0xe1); break; // ß
    case 0x00e9: out += char(0x82); break; // é
    case 0x00e8: out += char(0x8a); break; // è
    case 0x00b0: out += char(0xf8); break; // °
    case 0x2013: case 0x2014: out += '-'; break;
    case 0x2019: out += '\''; break;
    case 0x201e: case 0x201c: out += '"'; break;
    default: out += '?'; break;
    }
  }
  return out;
}

template <typename Graphics>
void printDisplayText(Graphics &gfx, const char *utf8) {
  gfx.print(displayGlyphs(utf8).c_str());
}
} // namespace leap
