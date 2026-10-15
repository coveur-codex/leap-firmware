#pragma once
#include "ChillMusic.h"
#include "Notifications.h"
#include "Storage.h"
#include <atomic>
namespace leap {
struct AudioJob {
  char path[100];
  uint16_t frequency, duration;
};
class Audio {
  QueueHandle_t jobs = nullptr;
  std::atomic<NotificationSound> notificationRequested{NotificationSound::Silent};

  std::atomic<MusicProfile> musicProfile{MusicProfile::Off};
  std::atomic<uint8_t> musicIntensity{45};
  std::atomic<uint32_t> pictureEvents{0};

public:
  std::atomic<uint8_t> volume{35};
  std::atomic<bool> stopRequested{false};
  bool begin();
  void music(MusicProfile profile, uint8_t intensity = 45) {
    musicIntensity = intensity;
    musicProfile = profile;
  }
  void pictureCue(PictureCue cue) {
    pictureEvents.fetch_or(1u << unsigned(cue));
  }
  void notify(NotificationSound sound = NotificationSound::Pling) {
    notificationRequested = sound;
  }
  void tone(uint16_t hz, uint16_t duration = 120);
  bool play(const String &blob);
  void stop() {
    stopRequested = true;
  }
  static bool validateWav(const String &blob);

private:
  void run();
};
extern Audio audio;
} // namespace leap
