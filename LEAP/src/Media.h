#pragma once
#include "Storage.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class Media {
  String cached;
  int cw = 0, ch = 0;
  uint16_t *pixels = nullptr;

public:
  ~Media() {
    free(pixels);
  }
  bool draw(Arduino_GFX &gfx, const String &blob, const String &original, int x, int y, int width,
            int height);
  static bool avatarPng(const String &blob);
  static bool validate(const String &blob, const String &name);
  static uint16_t *decode(const String &blob, const String &name, int width, int height);
};
} // namespace leap
