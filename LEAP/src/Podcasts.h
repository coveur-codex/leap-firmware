#pragma once
#include "Input.h"
#include "Storage.h"
#include <atomic>
namespace leap {
class Transport;
enum class PodcastRequestKind : uint8_t { Resume, List, Episode, Next };
struct PodcastReply {
  JsonDocument data{&jsonRam};
  uint32_t ticket = 0;
  PodcastRequestKind kind = PodcastRequestKind::List;
  int http = 0;
  bool cached = false;
};
class PodcastInbox {
  struct Request { String path; uint32_t ticket; PodcastRequestKind kind; };
  std::atomic<Request *> pending{nullptr};
  std::atomic<PodcastReply *> ready{nullptr};
  QueueHandle_t updates = nullptr;
  JsonDocument journal{&jsonRam}; // Owned exclusively by existing Network worker.
  bool loaded = false, dirty = false;
  uint32_t lastRetry = 0;
  uint32_t serial = 0;
  void overlay(JsonVariant episode);
  bool restore(const String &path, JsonDocument &out);
  void cache(const String &path, JsonDocument &value);
public:
  std::atomic<bool> storageError{false};
  bool begin();
  uint32_t request(const String &suffix, PodcastRequestKind kind);
  bool save(JsonVariantConst episode, uint32_t position, uint32_t duration, bool completed=false);
  PodcastReply *take() { return ready.exchange(nullptr); }
  void service(Transport &net, const String &base, bool connected);
};
extern PodcastInbox podcastInbox;
class Podcasts {
  bool visible = false, ended = false, playing = false;
  uint32_t ticket = 0, lastSaved = 0, savedPosition=UINT32_MAX, savedDuration=0;
  String savedId;
  bool savedCompleted=false;
  PodcastRequestKind waiting = PodcastRequestKind::List;
  bool startAfterList = false;
  int wantedIndex = 0;
  void request(const String &suffix, PodcastRequestKind kind);
  void loadList(int index, bool start=false);
  void start(JsonVariantConst episode, int index);
  void save(bool completed=false);
  bool allowed(int id) const;
public:
  enum class View { Channels, Episodes, Playback };
  View view = View::Channels;
  JsonDocument channels{&jsonRam}, page{&jsonRam}, current{&jsonRam};
  int channel = 0, selected = 0, total = 0, currentIndex = 0;
  String message;
  bool busy = false;
  void show(bool shown, JsonArrayConst granted);
  void tick();
  void action(const InputEvent &event);
  bool playbackVisible() const { return visible && view == View::Playback; }
  uint32_t position() const;
  uint32_t duration() const;
  String channelName(int id) const;
};
} // namespace leap
