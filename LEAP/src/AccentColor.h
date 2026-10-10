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
} // namespace leap
