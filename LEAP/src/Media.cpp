#include "Media.h"
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
// PNGdec keeps two scanlines, alignment padding and an optional fast palette.
// build_opt.h applies this size to both the sketch and the library implementation.
static_assert(PNG_MAX_BUFFERED_PIXELS >= 2 * (1024 * 4 + 32) + 512,
              "Keep LEAP/build_opt.h in the sketch: PNGdec must support 1024px RGBA rows");
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
uint16_t *Media::decode(const String &path, const String &name, int width, int height,
                        int *fittedWidth, int *fittedHeight) {
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
  auto fit = [&]() {
    if (fittedWidth && fittedHeight && iw > 0 && ih > 0 && iw <= 1024 && ih <= 1024) {
      if (iw * height > ih * width)
        oh = std::max(1, ih * width / iw);
      else
        ow = std::max(1, iw * height / ih);
    }
  };
  String ext = name;
  ext.toLowerCase();
  if (ext.endsWith(".png")) {
    int code = png.open(path.c_str(), openFile, closeFile, pngRead, pngSeek, drawPng);
    if (code == PNG_SUCCESS) {
      iw = png.getWidth();
      ih = png.getHeight();
      fit();
      code = iw > 0 && ih > 0 && iw <= 1024 && ih <= 1024 ? png.decode(nullptr, 0) : PNG_TOO_BIG;
      ok = code == PNG_SUCCESS;
      if (!ok)
        Serial.printf("[%10lu] [MEDIA   ] PNG decode failed: %s code=%d size=%dx%d\n",
                      millis(), name.c_str(), code, iw, ih);
    } else
      Serial.printf("[%10lu] [MEDIA   ] PNG open failed: %s code=%d\n", millis(), name.c_str(), code);
    png.close();
  } else if (ext.endsWith(".jpg") || ext.endsWith(".jpeg")) {
    if (jpeg.open(path.c_str(), openFile, closeFile, jpgRead, jpgSeek, drawJpg)) {
      iw = jpeg.getWidth();
      ih = jpeg.getHeight();
      fit();
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
  if (fittedWidth) *fittedWidth = ow;
  if (fittedHeight) *fittedHeight = oh;
  xSemaphoreGive(codecMutex);
  return result;
}
bool Media::avatarPng(const String &path, uint32_t *width, uint32_t *height) {
  if (width) *width = 0;
  if (height) *height = 0;
  File f = LittleFS.open(path, "r");
  uint8_t h[24];
  if (!f || f.read(h, sizeof(h)) != sizeof(h))
    return false;
  const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (memcmp(h, signature, 8) || memcmp(h + 12, "IHDR", 4))
    return false;
  uint32_t w = uint32_t(h[16]) << 24 | uint32_t(h[17]) << 16 | uint32_t(h[18]) << 8 | h[19];
  uint32_t heightValue = uint32_t(h[20]) << 24 | uint32_t(h[21]) << 16 | uint32_t(h[22]) << 8 | h[23];
  if (width) *width = w;
  if (height) *height = heightValue;
  return w == 80 && heightValue == 80;
}
bool Media::validate(const String &path, const String &name) {
  auto *data = decode(path, name, 8, 8);
  bool ok = data;
  free(data);
  return ok;
}
bool Media::draw(Arduino_GFX &gfx, const String &path, const String &original, int x, int y, int w,
                 int h, bool fit) {
  if (!path.length())
    return false;
  if (cached != path || cw != w || ch != h || cachedFit != fit) {
    free(pixels);
    pw = w;
    ph = h;
    pixels = decode(path, original, w, h, fit ? &pw : nullptr, fit ? &ph : nullptr);
    cached = path;
    cw = w;
    ch = h;
    cachedFit = fit;
  }
  if (!pixels)
    return false;
  gfx.draw16bitRGBBitmap(x + (w - pw) / 2, y + (h - ph) / 2, pixels, pw, ph);
  return true;
}
} // namespace leap
