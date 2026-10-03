#include "Media.h"
#include "MediaColor.h"
#include "MediaMask.h"
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <new>
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
static uint8_t *opacity;
static int ow, oh, iw, ih;
static uint32_t backgroundColor, lastYield;
struct MediaJob {
  std::atomic<unsigned> references{2}; // UI owner and decoder worker.
  std::atomic<bool> done{false};
  String path, name;
  int width, height, fittedWidth = 0, fittedHeight = 0;
  bool fit, transparent = false;
  uint16_t background;
  uint16_t *pixels = nullptr;
  uint8_t *alpha = nullptr;
  ~MediaJob() {
    free(pixels);
    free(alpha);
  }
};
static QueueHandle_t decodeRequests = nullptr;
static void releaseJob(MediaJob *job) {
  if (job && job->references.fetch_sub(1) == 1)
    delete job;
}
static void decodeWorker(void *) {
  for (;;) {
    MediaJob *job;
    if (xQueueReceive(decodeRequests, &job, portMAX_DELAY) != pdTRUE)
      continue;
    job->pixels = Media::decode(job->path, job->name, job->width, job->height,
                                job->fit ? &job->fittedWidth : nullptr,
                                job->fit ? &job->fittedHeight : nullptr, job->background,
                                job->transparent ? &job->alpha : nullptr);
    if (!job->fit) {
      job->fittedWidth = job->width;
      job->fittedHeight = job->height;
    }
    job->done.store(true, std::memory_order_release);
    releaseJob(job);
  }
}
bool Media::beginWorker() {
  if (decodeRequests)
    return true;
  decodeRequests = xQueueCreate(4, sizeof(MediaJob *));
  if (!decodeRequests)
    return false;
  if (xTaskCreatePinnedToCore(decodeWorker, "leap-media", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    vQueueDelete(decodeRequests);
    decodeRequests = nullptr;
    return false;
  }
  return true;
}
Media::~Media() {
  releaseJob(pending);
  free(pixels);
  free(alpha);
}
static void codecYield() {
  if (uint32_t(millis() - lastYield) >= 8) {
    vTaskDelay(1);
    lastYield = millis();
  }
}
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
  int from = (row->y * oh + ih - 1) / ih, to = ((row->y + 1) * oh + ih - 1) / ih;
  if (from == to) {
    codecYield();
    return 1;
  }
  uint16_t line[1024];
  png.getLineAsRGB565(row, line, PNG_RGB565_LITTLE_ENDIAN, backgroundColor);
  for (int y = from; y < to && y < oh; y++)
    for (int x = 0; x < ow; x++) {
      int source = x * iw / ow;
      output[y * ow + x] = line[source];
      if (opacity)
        opacity[y * ow + x] = pngOpaque(*row, source, png.getTransparentColor()) ? 1 : 0;
    }
  codecYield();
  return 1;
}
static int drawJpg(JPEGDRAW *block) {
  // Visit only destination pixels covered by this MCU block.
  int top = (block->y * oh + ih - 1) / ih;
  int bottom = std::min(oh, ((block->y + block->iHeight) * oh + ih - 1) / ih);
  int left = (block->x * ow + iw - 1) / iw;
  int right = std::min(ow, ((block->x + block->iWidthUsed) * ow + iw - 1) / iw);
  for (int y = top; y < bottom; y++) {
    int sy = y * ih / oh - block->y;
    if (sy < 0 || sy >= block->iHeight)
      continue;
    for (int x = left; x < right; x++) {
      int sx = x * iw / ow - block->x;
      if (sx >= 0 && sx < block->iWidthUsed)
        output[y * ow + x] = block->pPixels[sy * block->iWidth + sx];
    }
  }
  codecYield();
  return 1;
}
uint16_t *Media::decode(const String &path, const String &name, int width, int height,
                        int *fittedWidth, int *fittedHeight, uint16_t background, uint8_t **mask) {
  if (width < 1 || height < 1 || width > 428 || height > 142 || !codecMutex)
    return nullptr;
  xSemaphoreTake(codecMutex, portMAX_DELAY);
  backgroundColor = pngBackground(background);
  lastYield = millis();
  ow = width;
  oh = height;
  output =
      static_cast<uint16_t *>(heap_caps_calloc(ow * oh, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!output) {
    xSemaphoreGive(codecMutex);
    return nullptr;
  }
  opacity = mask ? static_cast<uint8_t *>(
                       heap_caps_calloc(ow * oh, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))
                 : nullptr;
  if (mask && !opacity) {
    free(output);
    output = nullptr;
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
  if (mask) {
    if (!ok) {
      free(opacity);
      opacity = nullptr;
    }
    *mask = opacity;
  }
  opacity = nullptr;
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
                 int h, bool fit, uint16_t background, bool asynchronous, bool retainFrame,
                 bool transparent) {
  if (!path.length())
    return false;
  if (pending && pending->done.load(std::memory_order_acquire)) {
    if (pending->width == w && pending->height == h && pending->fit == fit &&
        pending->background == background && pending->transparent == transparent &&
        (pending->path == path || retainFrame)) {
      free(pixels);
      free(alpha);
      alpha = pending->alpha;
      pending->alpha = nullptr;
      pixels = pending->pixels;
      pending->pixels = nullptr;
      pw = pending->fittedWidth;
      ph = pending->fittedHeight;
      cached = pending->path;
      cw = w;
      ch = h;
      cachedFit = fit;
      cachedBackground = background;
      cachedTransparent = transparent;
    }
    releaseJob(pending);
    pending = nullptr;
  }
  bool compatible = cw == w && ch == h && cachedFit == fit && cachedBackground == background &&
                    cachedTransparent == transparent;
  if (cached != path || !compatible) {
    if (asynchronous) {
      if (!pending && decodeRequests) {
        auto *job = new (std::nothrow) MediaJob;
        if (job) {
          job->path = path;
          job->name = original;
          job->width = w;
          job->height = h;
          job->fit = fit;
          job->background = background;
          job->transparent = transparent;
          if (xQueueSend(decodeRequests, &job, 0) == pdTRUE)
            pending = job;
          else
            delete job; // No worker owns a request that was never enqueued.
        }
      }
    } else {
      free(pixels);
      free(alpha);
      alpha = nullptr;
      pw = w;
      ph = h;
      pixels = decode(path, original, w, h, fit ? &pw : nullptr, fit ? &ph : nullptr, background,
                      transparent ? &alpha : nullptr);
      cached = path;
      cw = w;
      ch = h;
      cachedFit = fit;
      cachedBackground = background;
      cachedTransparent = transparent;
      compatible = true;
    }
  }
  if (!pixels || !compatible || (!retainFrame && cached != path))
    return false;
  if (transparent && alpha) {
    for (int row = 0; row < ph; ++row)
      for (int col = 0; col < pw; ++col)
        if (alpha[row * pw + col])
          gfx.drawPixel(x + (w - pw) / 2 + col, y + (h - ph) / 2 + row, pixels[row * pw + col]);
  } else
    gfx.draw16bitRGBBitmap(x + (w - pw) / 2, y + (h - ph) / 2, pixels, pw, ph);
  return true;
}
} // namespace leap
