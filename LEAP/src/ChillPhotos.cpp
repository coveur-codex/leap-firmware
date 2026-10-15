#include "ChillPhotos.h"
#include "Audio.h"
#include "Transport.h"
namespace leap {
PhotoInbox photoInbox;
void PhotoInbox::service(Transport &net, const String &base, bool connected) {
  if (!active.load())
    delete ready.exchange(nullptr);
  uint32_t id = requested.exchange(0);
  if (!id)
    return;
  PhotoFrame *frame = nullptr;
  // Restore once per session; only successful new photos replace this bounded cache.
  static uint32_t restored = 0;
  if (id != restored && active == id) {
    restored = id;
    auto *pixels = Media::decode("/chill-photo.jpg", "photo.jpg", 428, 142);
    if (pixels)
      frame = new PhotoFrame{id, pixels};
  }
  if (!frame && connected && active == id) {
    log("PHOTO", "Prefetch started");
    JsonDocument info(&jsonRam);
    if (storage.freeBytes() > ReserveBytes + 128 * 1024 &&
        net.json(base + "/chill/photo", info, nullptr, 30000) && info["width"] == 428 &&
        info["height"] == 142 && info["size"].is<unsigned>() &&
        info["size"].as<unsigned>() <= 128 * 1024) {
      File f = LittleFS.open("/chill-photo.part", "w");
      bool ok =
          f && net.download(info["url"].as<String>(), info["size"], info["sha256"].as<String>(),
                            [&](const uint8_t *p, size_t n) { return f.write(p, n) == n; });
      f.close();
      auto *pixels = ok ? Media::decode("/chill-photo.part", "photo.jpg", 428, 142) : nullptr;
      if (pixels) {
        frame = new PhotoFrame{id, pixels};
        LittleFS.rename("/chill-photo.part", "/chill-photo.jpg");
      }
      LittleFS.remove("/chill-photo.part");
      storage.refreshSpace();
    }
    log("PHOTO", frame ? "Decoded photo ready" : "Prefetch failed; retaining current image");
  }
  if (active == id)
    delete ready.exchange(frame);
  else
    delete frame;
  busy = false;
}
void ChillPhotos::start(uint32_t now, int value) {
  close();
  session = photoInbox.open();
  transition.reset(now, value);
  retryAt = now;
  backoff = 10000;
  attempted = false;
}
void ChillPhotos::close() {
  if (!session)
    return;
  photoInbox.close();
  session = 0;
  delete current;
  current = nullptr;
  delete next;
  next = nullptr;
}
void ChillPhotos::tick(uint32_t now) {
  if (!session)
    return;
  if (auto *frame = photoInbox.take()) {
    if (frame->session == session) {
      delete next;
      next = frame;
      backoff = 10000;
    } else
      delete frame;
  }
  if (!next && !photoInbox.busy && transition.prefetch(now, current != nullptr) &&
      (!attempted || int32_t(now - retryAt) >= 0)) {
    photoInbox.request(session);
    attempted = true;
    retryAt = now + backoff;
    backoff = std::min(backoff * 2, uint32_t(300000));
  }
  auto cue = transition.tick(now, next != nullptr, current != nullptr);
  if (cue != PictureCue::None) {
    audio.pictureCue(cue);
    if (cue == PictureCue::FadeOut)
      log("PHOTO", "Image transition started");
  }
  if (transition.swap) {
    delete current;
    current = next;
    next = nullptr;
    attempted = false;
  }
}
void ChillPhotos::draw(Arduino_GFX &gfx, uint32_t now) {
  gfx.fillScreen(0);
  if (!current)
    return;
  uint8_t gain = transition.brightness(now);
  uint16_t row[428];
  for (int y = 0; y < 142; ++y) {
    for (int x = 0; x < 428; ++x)
      row[x] = PictureTransition::dim(current->pixels[y * 428 + x], gain);
    gfx.draw16bitRGBBitmap(0, y, row, 428, 1);
  }
}
} // namespace leap
