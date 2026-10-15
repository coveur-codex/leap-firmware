#include "Audio.h"
#include "Hardware.h"
#include "NotificationTone.h"
#include <driver/i2s_std.h>
namespace leap {
Audio audio;
struct Wav {
  uint32_t rate = 0, offset = 0, size = 0;
  uint16_t channels = 0;
};
static uint32_t little32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static bool wavHeader(File &f, Wav &w) {
  uint8_t head[12];
  if (f.read(head, 12) != 12 || memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4))
    return false;
  bool format = false;
  while (f.available()) {
    uint8_t chunk[8];
    if (f.read(chunk, 8) != 8)
      return false;
    uint32_t n = little32(chunk + 4), pos = f.position();
    if (n > f.size() - pos)
      return false;
    if (!memcmp(chunk, "fmt ", 4)) {
      uint8_t fmt[16];
      if (n < 16 || f.read(fmt, 16) != 16)
        return false;
      w.channels = fmt[2] | (fmt[3] << 8);
      w.rate = little32(fmt + 4);
      format = fmt[0] == 1 && fmt[1] == 0 && fmt[14] == 16 && fmt[15] == 0 &&
               (w.channels == 1 || w.channels == 2) && w.rate >= 8000 && w.rate <= 48000;
    } else if (!memcmp(chunk, "data", 4) && format) {
      w.offset = pos;
      w.size = n;
      return n > 0 && n % (2 * w.channels) == 0;
    }
    if (!f.seek(pos + n + (n & 1)))
      return false;
  }
  return false;
}
bool Audio::validateWav(const String &path) {
  File f = LittleFS.open(path, "r");
  Wav w;
  return f && wavHeader(f, w);
}
bool Audio::begin() {
  jobs = xQueueCreate(4, sizeof(AudioJob));
  return jobs && xTaskCreatePinnedToCore([](void *p) { static_cast<Audio *>(p)->run(); },
                                         "leap-audio", 6144, this, 1, nullptr, 0) == pdPASS;
}
void Audio::tone(uint16_t hz, uint16_t duration) {
  if (jobs) {
    AudioJob j{};
    j.frequency = hz;
    j.duration = duration;
    xQueueSend(jobs, &j, 0);
  }
}
bool Audio::play(const String &path) {
  if (!jobs || path.length() >= 100)
    return false;
  AudioJob j{};
  strlcpy(j.path, path.c_str(), sizeof(j.path));
  return xQueueSend(jobs, &j, 0) == pdTRUE;
}
void Audio::run() {
  i2s_chan_handle_t tx = nullptr;
  i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&channel, &tx, nullptr) != ESP_OK) {
    log("AUDIO", "I2S channel failed");
    vTaskDelete(nullptr);
    return;
  }
  i2s_std_config_t config{};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050);
  config.slot_cfg =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = gpio_num_t(hw::Bclk);
  config.gpio_cfg.ws = gpio_num_t(hw::Ws);
  config.gpio_cfg.dout = gpio_num_t(hw::Din);
  config.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx, &config) != ESP_OK) {
    log("AUDIO", "I2S init failed");
    i2s_del_channel(tx);
    vTaskDelete(nullptr);
    return;
  }
  log("AUDIO", "I2S ready; eight-voice generative synth, 22050 Hz default");
  NotificationTone notification;
  ChillMusic music;
  music.seed(esp_random());
  File file;
  Wav wav;
  AudioJob job{};
  uint32_t rate = 22050, remaining = 0, phase = 0;
  bool enabled = false;
  MusicProfile previous = MusicProfile::Off;
  for (;;) {
    auto requestedProfile = musicProfile.load();
    if (requestedProfile != previous) {
      Serial.printf("[AUDIO] Chill profile %u -> %u\n", unsigned(previous),
                    unsigned(requestedProfile));
      previous = requestedProfile;
    }
    music.request(requestedProfile, musicIntensity.load());
    uint32_t cues = pictureEvents.exchange(0);
    for (unsigned cue = 1; cue <= unsigned(PictureCue::Complete); ++cue)
      if (cues & (1u << cue))
        music.cue(PictureCue(cue));
    if (stopRequested.exchange(false)) {
      remaining = 0;
      file.close();
    }
    uint32_t nextRate = rate;
    if (!remaining) {
      file.close();
      if (xQueueReceive(jobs, &job, music.active() ? 0 : pdMS_TO_TICKS(20)) == pdTRUE) {
        nextRate = 22050;
        remaining = nextRate * job.duration / 1000;
        phase = 0;
        if (job.path[0]) {
          file = LittleFS.open(job.path, "r");
          if (!file || !wavHeader(file, wav)) {
            file.close();
            remaining = 0;
          } else {
            nextRate = wav.rate;
            remaining = wav.size / (2 * wav.channels);
            file.seek(wav.offset);
          }
        }
      } else
        nextRate = 22050;
    }
    auto sound = notificationRequested.exchange(NotificationSound::Silent);
    if (sound != NotificationSound::Silent)
      notification.start(sound);
    if (!remaining && !notification.active() && !music.active()) {
      if (enabled) {
        int16_t silence[256]{};
        size_t written;
        i2s_channel_write(tx, silence, sizeof(silence), &written, 200);
        i2s_channel_disable(tx);
        enabled = false;
      }
      continue;
    }
    if (!enabled || nextRate != rate) {
      if (enabled)
        i2s_channel_disable(tx);
      rate = nextRate;
      i2s_std_clk_config_t clock = I2S_STD_CLK_DEFAULT_CONFIG(rate);
      i2s_channel_reconfig_std_clock(tx, &clock);
      i2s_channel_enable(tx);
      enabled = true;
    }
    int16_t buffer[256];
    for (size_t i = 0; i < 128; ++i) {
      int16_t l = 0, r = 0;
      if (remaining) {
        if (file) {
          if (file.read(reinterpret_cast<uint8_t *>(&l), 2) != 2)
            remaining = 0;
          r = l;
          if (wav.channels == 2 && file.read(reinterpret_cast<uint8_t *>(&r), 2) != 2)
            remaining = 0;
        } else {
          phase = (phase + job.frequency) % rate;
          l = r = job.frequency ? (phase < rate / 2 ? 3000 : -3000) : 0;
        }
        if (remaining)
          --remaining;
      }
      int16_t ambient = music.next(rate), chime = notification.next(rate);
      buffer[2 * i] = int32_t(NotificationTone::mix(NotificationTone::mix(l, ambient), chime)) *
                      volume.load() / 100;
      buffer[2 * i + 1] = int32_t(NotificationTone::mix(NotificationTone::mix(r, ambient), chime)) *
                          volume.load() / 100;
    }
    size_t written = 0;
    if (i2s_channel_write(tx, buffer, sizeof(buffer), &written, 200) != ESP_OK ||
        written != sizeof(buffer)) {
      log("AUDIO", "I2S write failed");
      remaining = 0;
      file.close();
      music.request(MusicProfile::Off, 0);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}
} // namespace leap
