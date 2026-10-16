#pragma once
#include <cstdint>
#include <cstring>
namespace leap {
inline uint16_t accentColor(const char *hex) {
  if (!hex || std::strlen(hex) != 7 || hex[0] != '#') return 0x06b8;
  unsigned rgb = 0;
  for (int i = 1; i < 7; ++i) {
    char c = hex[i];
    int digit = c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1;
    if (digit < 0) return 0x06b8;
    rgb = (rgb << 4) | digit;
  }
  return ((rgb >> 19) << 11) | (((rgb >> 10) & 63) << 5) | ((rgb >> 3) & 31);
}
// Relative RGB565 luminance for readable text on an arbitrary configured color.
inline uint16_t contrastInk(uint16_t color) {
  int r = (color >> 11) * 255 / 31;
  int g = ((color >> 5) & 63) * 255 / 63;
  int b = (color & 31) * 255 / 31;
  return 299*r + 587*g + 114*b >= 140000 ? 0x0841 : 0xffff;
}
} // namespace leap
