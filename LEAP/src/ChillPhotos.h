#pragma once
#include "Media.h"
#include "PictureTransition.h"
#include <atomic>
#include <vector>
namespace leap {
class Transport;
struct PhotoFrame {
  uint32_t session;
  uint16_t *pixels;
  ~PhotoFrame() {
    free(pixels);
  }
};
// Single slot mailboxes between existing network worker and UI. No blocking UI calls.
class PhotoInbox {
  std::atomic<uint32_t> active{0}, requested{0};
  std::atomic<PhotoFrame *> ready{nullptr};
  std::atomic<String *> localSource{nullptr};
  uint32_t serial = 0;

public:
  std::atomic<bool> busy{false};
  uint32_t open() {
    uint32_t id = ++serial;
    if (!id)
      id = ++serial;
    active = id;
    return id;
  }
  void close() {
    active = 0;
    delete ready.exchange(nullptr);
  }
  void request(uint32_t id, const String &blob = "") {
    if (!busy.exchange(true)) {
      localSource = new String(blob);
      requested = id;
    }
  }
  PhotoFrame *take() {
    return ready.exchange(nullptr);
  }
  void service(Transport &net, const String &base, bool connected);
};
extern PhotoInbox photoInbox;
class ChillPhotos {
  PhotoFrame *current = nullptr, *next = nullptr;
  uint32_t session = 0, retryAt = 0, backoff = 10000;
  bool attempted = false;
  PictureTransition transition;
  std::vector<String> album;
  size_t albumIndex = 0;

public:
  ~ChillPhotos() {
    close();
  }
  void start(uint32_t now, int value);
  void startAlbum(uint32_t now, int value, std::vector<String> sources);
  void close();
  void adjust(int value, uint32_t now) {
    transition.adjust(value, now);
  }
  void tick(uint32_t now);
  void draw(Arduino_GFX &gfx, uint32_t now);
};
} // namespace leap
