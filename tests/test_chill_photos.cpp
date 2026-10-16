#include "Audio.h"
#include "ChillPhotos.h"
#include "Transport.h"
#include <cassert>
#include <functional>
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) {
  return malloc(n);
}
void RamAllocator::deallocate(void *p) {
  free(p);
}
void *RamAllocator::reallocate(void *p, size_t n) {
  return realloc(p, n);
}
Storage storage;
Audio audio;
size_t Storage::freeBytes() const {
  return 1024 * 1024;
}
void Storage::refreshSpace() {}
static bool online = true, valid = true;
static unsigned requests = 0;
static uint16_t color = 0xf800;
static std::function<void()> onDownload;
bool Transport::json(const String &, JsonDocument &info, JsonDocument *, uint32_t timeout) {
  assert(timeout == 30000);
  ++requests;
  info["width"] = 428;
  info["height"] = 142;
  info["size"] = 4;
  info["url"] = "/api/photo";
  info["sha256"] = "hash";
  return online;
}
bool Transport::download(const String &, size_t, const String &,
                         const std::function<bool(const uint8_t *, size_t)> &sink) {
  if (onDownload)
    onDownload();
  const uint8_t bytes[4]{};
  return online && sink(bytes, sizeof(bytes));
}
uint16_t *Media::decode(const String &path, const String &, int w, int h, int *, int *, uint16_t,
                        uint8_t **) {
  if (!valid || !LittleFS.exists(path))
    return nullptr;
  auto *pixels = static_cast<uint16_t *>(malloc(w * h * 2));
  std::fill(pixels, pixels + w * h, color);
  return pixels;
}
} // namespace leap
int main() {
  using namespace leap;
  Transport transport;
  Arduino_GFX gfx;
  ChillPhotos photos;
  photos.start(0, 300);
  photos.tick(0);
  photoInbox.service(transport, "/api/v1/devices/test", true);
  assert(requests == 1);
  for (uint32_t now : {100u, 1300u, 2800u, 3100u, 4600u})
    photos.tick(now);
  photos.draw(gfx, 4600);
  assert(gfx.pixels[1000] == 0xf800);
  online = false;
  for (uint32_t now = 16000; now < 316000; now += 100) {
    photos.tick(now);
    photoInbox.service(transport, "/api/v1/devices/test", true);
    photos.draw(gfx, now);
    assert(gfx.pixels[1000] == 0xf800);
  }
  assert(requests < 10); // exponential retry, no black fade during a failed download
  online = true;
  valid = false;
  photos.tick(640000);
  photoInbox.service(transport, "/api/v1/devices/test", true);
  photos.tick(640100);
  photos.draw(gfx, 640100);
  assert(gfx.pixels[1000] == 0xf800);
  valid = true;
  color = 0x07e0;
  photos.tick(950000);
  photoInbox.service(transport, "/api/v1/devices/test", true);
  photos.tick(950100);
  photos.tick(951300);
  photos.tick(952800);
  photos.draw(gfx, 952800);
  assert(gfx.pixels[1000] == 0);
  photos.tick(953100);
  photos.tick(954600);
  photos.draw(gfx, 954600);
  assert(gfx.pixels[1000] == 0x07e0);
  photos.close();
  online = false;
  photos.start(1000000, 200);
  photos.tick(1000000);
  unsigned before = requests;
  photoInbox.service(transport, "/api/v1/devices/test", false);
  for (uint32_t now : {1000100u, 1001300u, 1002800u, 1003100u, 1004600u})
    photos.tick(now);
  photos.draw(gfx, 1004600);
  assert(gfx.pixels[1000] == 0x07e0 && requests == before);
  photos.close();
  fakeFiles.clear();
  online = true;
  photos.start(2000000, 300);
  photos.tick(2000000);
  onDownload = [&] { photos.close(); }; // leave while network worker owns the download
  photoInbox.service(transport, "/api/v1/devices/test", true);
  assert(!photoInbox.busy && photoInbox.take() == nullptr);
  onDownload = nullptr;
  photos.start(2001000, 300);
  photos.tick(2001000);
  photos.close(); // cancel an enqueued request before the worker gets it
  photoInbox.service(transport, "/api/v1/devices/test", true);
  assert(!photoInbox.busy);
  // Local albums are decoded by the same worker, without any HTTP request or cache writes.
  online = false; color = 0x001f;
  fakeFiles["/chill-photo.jpg"] = std::make_shared<std::string>("network-cache");
  fakeFiles["/blob/album-a"] = std::make_shared<std::string>("a");
  fakeFiles["/blob/album-b"] = std::make_shared<std::string>("b");
  before = requests;
  photos.startAlbum(3000000, 300, {String("/blob/album-a"), String("/blob/album-b")});
  photos.tick(3000000);
  photoInbox.service(transport, "/api/v1/devices/test", false);
  for (uint32_t now : {3000100u,3001300u,3002800u,3003100u,3004600u}) photos.tick(now);
  photos.draw(gfx,3004600); assert(gfx.pixels[1000] == 0x001f && requests == before);
  assert(*fakeFiles.at("/chill-photo.jpg") == "network-cache");
  color = 0xffe0; photos.tick(3016300);
  photoInbox.service(transport, "/api/v1/devices/test", false);
  for (uint32_t now : {3030100u,3031300u,3032800u,3033100u,3034600u}) photos.tick(now);
  photos.draw(gfx,3034600); assert(gfx.pixels[1000] == 0xffe0 && requests == before);
  photos.close();
  photos.startAlbum(4000000, 200, {String("/blob/album-a")}); photos.tick(4000000); photos.close();
  photoInbox.service(transport, "", false); assert(!photoInbox.busy && !photoInbox.take());
  puts("PASS: real photo lifecycle, rendering, offline/backoff, invalid decode, black swap, cache "
       "restore and in-flight cancellation");
}
