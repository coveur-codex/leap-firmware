#include "Media.h"
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
namespace leap {
// Only one decode at a time; callback context never escapes this mutex.
static SemaphoreHandle_t codecMutex = xSemaphoreCreateMutex();
static PNG png;
static JPEGDEC jpeg;
static File input;
static uint16_t *output;
static int ow, oh, iw, ih;
static void *openFile(const char *name, int32_t *size) {
  input = LittleFS.open(name, "r");
  *size = input ? input.size() : 0;
  return input ? &input : nullptr;
}
static void closeFile(void *) {
  input.close();
}
static int32_t pngRead(PNGFILE *, uint8_t *p, int32_t n) {
  return input.read(p, n);
}
static int32_t pngSeek(PNGFILE *, int32_t pos) {
  return input.seek(pos) ? pos : 0;
}
static int32_t jpgRead(JPEGFILE *, uint8_t *p, int32_t n) {
  return input.read(p, n);
}
static int32_t jpgSeek(JPEGFILE *, int32_t pos) {
  return input.seek(pos) ? pos : 0;
}
static int drawPng(PNGDRAW *row) {
  if (row->iWidth > 1024)
    return 0;
  uint16_t line[1024];
  png.getLineAsRGB565(row, line, PNG_RGB565_LITTLE_ENDIAN, 0x101c2c);
  int from = (row->y * oh + ih - 1) / ih, to = ((row->y + 1) * oh + ih - 1) / ih;
  for (int y = from; y < to && y < oh; y++)
    for (int x = 0; x < ow; x++)
      output[y * ow + x] = line[x * iw / ow];
  return 1;
}
static int drawJpg(JPEGDRAW *block) {
  for (int y = 0; y < oh; y++) {
    int sy = y * ih / oh - block->y;
    if (sy < 0 || sy >= block->iHeight)
      continue;
    for (int x = 0; x < ow; x++) {
      int sx = x * iw / ow - block->x;
      if (sx >= 0 && sx < block->iWidthUsed)
        output[y * ow + x] = block->pPixels[sy * block->iWidth + sx];
    }
  }
  return 1;
}
uint16_t *Media::decode(const String &path, const String &name, int width, int height) {
  if (width < 1 || height < 1 || width > 428 || height > 142 || !codecMutex)
    return nullptr;
  xSemaphoreTake(codecMutex, portMAX_DELAY);
  ow = width;
  oh = height;
  output =
      static_cast<uint16_t *>(heap_caps_calloc(ow * oh, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!output) {
    xSemaphoreGive(codecMutex);
    return nullptr;
  }
  bool ok = false;
  String ext = name;
  ext.toLowerCase();
  if (ext.endsWith(".png")) {
    if (png.open(path.c_str(), openFile, closeFile, pngRead, pngSeek, drawPng) == PNG_SUCCESS) {
      iw = png.getWidth();
      ih = png.getHeight();
      ok = iw > 0 && ih > 0 && iw <= 1024 && ih <= 1024 && png.decode(nullptr, 0) == PNG_SUCCESS;
      png.close();
    }
  } else if (ext.endsWith(".jpg") || ext.endsWith(".jpeg")) {
    if (jpeg.open(path.c_str(), openFile, closeFile, jpgRead, jpgSeek, drawJpg)) {
      iw = jpeg.getWidth();
      ih = jpeg.getHeight();
      jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
      ok = iw > 0 && ih > 0 && iw <= 1024 && ih <= 1024 && jpeg.decode(0, 0, 0);
      jpeg.close();
    }
  }
  auto *result = output;
  if (!ok) {
    free(result);
    result = nullptr;
  }
  output = nullptr;
  xSemaphoreGive(codecMutex);
  return result;
}
bool Media::avatarPng(const String &path) {
  File f = LittleFS.open(path, "r");
  uint8_t h[24];
  if (!f || f.read(h, sizeof(h)) != sizeof(h))
    return false;
  const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
  return !memcmp(h, signature, 8) && !memcmp(h + 12, "IHDR", 4) && h[16] == 0 && h[17] == 0 &&
         h[18] == 0 && h[19] == 80 && h[20] == 0 && h[21] == 0 && h[22] == 0 && h[23] == 80;
}
bool Media::validate(const String &path, const String &name) {
  auto *data = decode(path, name, 8, 8);
  bool ok = data;
  free(data);
  return ok;
}
bool Media::draw(Arduino_GFX &gfx, const String &path, const String &original, int x, int y, int w,
                 int h) {
  if (!path.length())
    return false;
  if (cached != path || cw != w || ch != h) {
    free(pixels);
    pixels = decode(path, original, w, h);
    cached = path;
    cw = w;
    ch = h;
  }
  if (!pixels)
    return false;
  gfx.draw16bitRGBBitmap(x, y, pixels, w, h);
  return true;
}
} // namespace leap
