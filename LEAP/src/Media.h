#pragma once
#include "Storage.h"
#include <Arduino_GFX_Library.h>
namespace leap {
struct MediaJob;
class Media {
  String cached;
  int cw = 0, ch = 0;
  int pw = 0, ph = 0;
  bool cachedFit = false, cachedTransparent = false;
  uint8_t *alpha = nullptr;
  uint16_t cachedBackground = 0;
  uint16_t *pixels = nullptr;
  MediaJob *pending = nullptr;

public:
  Media() = default;
  Media(const Media &) = delete;
  Media &operator=(const Media &) = delete;
  ~Media();
  static bool beginWorker();
  bool draw(Arduino_GFX &gfx, const String &blob, const String &original, int x, int y, int width,
            int height, bool fit = false, uint16_t background = 0x10e5, bool asynchronous = true,
            bool retainFrame = false, bool transparent = false);
  static bool avatarPng(const String &blob, uint32_t *width = nullptr, uint32_t *height = nullptr);
  static bool validate(const String &blob, const String &name);
  static uint16_t *decode(const String &blob, const String &name, int width, int height,
                          int *fittedWidth = nullptr, int *fittedHeight = nullptr,
                          uint16_t background = 0x10e5, uint8_t **mask = nullptr);
};
} // namespace leap
