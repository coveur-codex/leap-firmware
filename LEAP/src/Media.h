#pragma once
#include "Storage.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class Media {
  String cached;
  int cw = 0, ch = 0;
  int pw = 0, ph = 0;
  bool cachedFit = false;
  uint16_t *pixels = nullptr;

public:
  ~Media() {
    free(pixels);
  }
  bool draw(Arduino_GFX &gfx, const String &blob, const String &original, int x, int y, int width,
            int height, bool fit = false);
  static bool avatarPng(const String &blob, uint32_t *width = nullptr, uint32_t *height = nullptr);
  static bool validate(const String &blob, const String &name);
  static uint16_t *decode(const String &blob, const String &name, int width, int height,
                          int *fittedWidth = nullptr, int *fittedHeight = nullptr);
};
} // namespace leap
