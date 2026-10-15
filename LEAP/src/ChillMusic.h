#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace leap {
enum class MusicProfile : uint8_t { Off, Space, Fire, Snow, Photos };
enum class PictureCue : uint8_t { None, Prepare, FadeOut, Black, FadeIn, Complete };
struct ChillProfile {
  float bpm, activity, sustain;
  int root;
  std::array<float, 4> gains; // pad, pluck, bell, bass; independent instrument levels
};
inline constexpr std::array<ChillProfile, 5> ChillProfiles{
    {{48, 0, .5f, 48, {0, 0, 0, 0}},
     {38, .28f, .88f, 45, {.30f, .10f, .22f, .12f}},
     {54, .55f, .35f, 48, {.12f, .48f, .13f, .22f}},
     {44, .27f, .45f, 50, {.13f, .30f, .38f, .08f}},
     {40, .32f, .65f, 48, {.22f, .30f, .20f, .12f}}}};
// Fixed memory synthesizer. Only the audio task owns voices/composer state.
class ChillSynth {
  struct Voice {
    float phase = 0, frequency = 0, age = 0, hold = 0, gain = 0;
    float attack = 0, decay = 0, sustain = 0, release = 0;
    unsigned instrument = 0;
    bool active = false;
  };
  std::array<Voice, 8> voices{};
  std::array<float, 256> sine{};
  std::array<float, 128> frequencies{};

public:
  ChillSynth() {
    for (unsigned i = 0; i < sine.size(); ++i)
      sine[i] = std::sin(i * 6.2831853f / 256);
    for (unsigned i = 0; i < frequencies.size(); ++i)
      frequencies[i] = 440 * std::pow(2.f, (int(i) - 69) / 12.f);
  }
  void clear() {
    for (auto &v : voices)
      v.active = false;
  }
  unsigned activeVoices() const {
    unsigned n = 0;
    for (auto &v : voices)
      n += v.active;
    return n;
  }
  void note(int pitch, unsigned instrument, float hold, float velocity, const ChillProfile &p) {
    Voice *slot = nullptr;
    for (auto &v : voices)
      if (!v.active) {
        slot = &v;
        break;
      }
    // Do not steal a sounding voice: excess notes become musical pauses.
    if (!slot)
      return;
    instrument %= 4;
    *slot = {0,
             frequencies[std::clamp(pitch, 24, 100)],
             0,
             hold,
             velocity * p.gains[instrument],
             instrument == 0 ? .8f : .015f,
             instrument == 2 ? .9f : .3f,
             instrument == 0 ? p.sustain : .25f,
             instrument == 0 ? 1.8f : .8f,
             instrument,
             true};
  }
  int16_t next(float dt) {
    float mixed = 0;
    for (auto &v : voices) {
      if (!v.active)
        continue;
      v.age += dt;
      float envelope = v.age < v.attack ? v.age / v.attack
                       : v.age < v.attack + v.decay
                           ? 1 - (1 - v.sustain) * (v.age - v.attack) / v.decay
                           : v.sustain;
      if (v.age > v.hold)
        envelope *= std::max(0.f, 1 - (v.age - v.hold) / v.release);
      if (v.age >= v.hold + v.release) {
        v.active = false;
        continue;
      }
      v.phase += v.frequency * dt;
      v.phase -= std::floor(v.phase);
      unsigned index = unsigned(v.phase * 256) & 255;
      float wave = sine[index];
      if (v.instrument == 1)
        wave = .8f * wave + .2f * sine[(index * 2) & 255];
      if (v.instrument == 2)
        wave = .72f * wave + .28f * sine[(index * 3) & 255];
      if (v.instrument == 0)
        wave = .9f * wave + .1f * sine[(index * 2) & 255];
      mixed += wave * envelope * v.gain;
    }
    // Eight voices at bounded gains stay within headroom; clamp is the final guard.
    return int16_t(std::clamp(mixed * 6500.f, -24000.f, 24000.f));
  }
};
class ChillMusic {
  ChillSynth synth;
  MusicProfile profile = MusicProfile::Off, target = MusicProfile::Off;
  uint32_t rng = 1;
  float fade = 0, level = .45f, beatRemaining = 0, bpm = 45, openness = 1, desiredOpenness = 1;
  uint8_t step = 0, bar = 0, harmony = 0, melody = 7, chord = 0;
  std::array<int, 8> motif{{0, 1, -1, 2, 0, -1, 1, 0}};
  uint32_t random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  int pitch(int degree) const {
    static constexpr int major[] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int minor[] = {0, 2, 3, 5, 7, 8, 10};
    const int *scale = profile == MusicProfile::Space ? minor : major;
    degree = std::clamp(degree, 0, 27);
    return ChillProfiles[unsigned(profile)].root + scale[degree % 7] + 12 * (degree / 7);
  }
  void compose() {
    const auto &p = ChillProfiles[unsigned(profile)];
    float activity = p.activity * (.35f + level * 1.2f) * openness;
    if (step == 0) {
      // Hold harmony across several bars. Space develops most slowly at low intensity.
      unsigned chordBars = profile == MusicProfile::Space ? (level < .4f ? 8 : 4)
                           : profile == MusicProfile::Photos || profile == MusicProfile::Snow ? 4
                                                                                              : 2;
      if (bar % chordBars == 0) {
        // Functional progression, varied middle chords, dominant preparing a tonic return.
        if (chord % 4 == 3)
          harmony = 4;
        else if (chord % 4 == 0)
          harmony = 0;
        else
          harmony = random() % 2 ? 3 : 5;
        ++chord;
        float hold = 60.f / bpm * (chordBars * 4 - .5f);
        synth.note(pitch(harmony), 3, hold, .65f * openness, p);
        synth.note(pitch(harmony + 7), 0, hold, .65f * openness, p);
        if (level > .5f)
          synth.note(pitch(harmony + 9), 0, hold, .45f * openness, p);
      }
      if (bar % 4 == 0)
        motif[random() % 8] = int(random() % 5) - 2;
      ++bar; // wrapping counters are phrase memory, never a playback restart
    }
    if ((random() & 1023) < unsigned(activity * 1024)) {
      melody = uint8_t(std::clamp(int(melody) + motif[step], 7, 16));
      int degree = step == 7 ? harmony + 7 : melody;
      unsigned instrument = profile == MusicProfile::Snow ? 2 : (random() % 5 == 0 ? 2 : 1);
      synth.note(pitch(degree), instrument, 60.f / bpm * (.5f + (random() % 3) * .5f),
                 .45f + (random() % 30) / 100.f, p);
    }
    step = (step + 1) % 8;
  }

public:
  void seed(uint32_t value) {
    rng = value ? value : 1;
  }
  void request(MusicProfile next, uint8_t intensity) {
    target = next;
    desiredLevel =
        next == MusicProfile::Photos ? .45f : std::min(unsigned(intensity), 100u) / 100.f;
  }
  float desiredLevel = .45f;
  void cue(PictureCue cue) {
    if (profile != MusicProfile::Photos || target != profile)
      return;
    const auto &p = ChillProfiles[unsigned(profile)];
    if (cue == PictureCue::Prepare) {
      desiredOpenness = .7f;
      if (random() % 3)
        synth.note(pitch(harmony + 11), 0, 1.5f, .25f, p);
    } else if (cue == PictureCue::FadeOut)
      desiredOpenness = .3f;
    else if (cue == PictureCue::Black) {
      if (random() % 4)
        synth.note(pitch(harmony + 7 + (random() % 2) * 2), 2, 1.5f, .35f, p);
    } else if (cue == PictureCue::FadeIn) {
      desiredOpenness = .8f;
      synth.note(pitch(harmony + 9), 1, 2.f, .3f, p);
    } else if (cue == PictureCue::Complete)
      desiredOpenness = 1;
  }
  bool active() const {
    return target != MusicProfile::Off || fade > 0;
  }
  unsigned activeVoices() const {
    return synth.activeVoices();
  }
  int16_t next(uint32_t rate = 22050) {
    float dt = 1.f / rate;
    if (profile != target) {
      fade = std::max(0.f, fade - dt / .7f);
      if (fade == 0) {
        synth.clear();
        profile = target;
        step = bar = harmony = chord = 0;
        beatRemaining = 0;
        openness = desiredOpenness = 1;
      }
    } else if (profile != MusicProfile::Off)
      fade = std::min(1.f, fade + dt / 1.5f);
    if (profile == MusicProfile::Off)
      return 0;
    level += (desiredLevel - level) * dt / 2;
    openness += (desiredOpenness - openness) * dt / .4f;
    float desiredBpm =
        ChillProfiles[unsigned(profile)].bpm + level * (profile == MusicProfile::Photos ? 0 : 8);
    bpm += (desiredBpm - bpm) * dt / 4;
    beatRemaining -= dt;
    if (beatRemaining <= 0) {
      compose();
      beatRemaining += 30.f / bpm;
    }
    return int16_t(synth.next(dt) * fade * (.7f + .3f * openness));
  }
};
} // namespace leap
