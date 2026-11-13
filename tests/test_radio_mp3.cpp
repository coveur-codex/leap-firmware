#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3/minimp3.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
int main() {
  for (bool mono : {false, true}) {
    std::ifstream file(mono ? "tests/fixtures/radio/mono.mp3" : "tests/fixtures/radio/stereo.mp3", std::ios::binary);
    assert(file.good());
    std::vector<uint8_t> input{std::istreambuf_iterator<char>(file), {}};
    mp3dec_t decoder{}; mp3dec_init(&decoder);
    mp3d_sample_t output[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t offset = 0, frames = 0, samples = 0; double energy = 0;
    while (offset < input.size()) {
      mp3dec_frame_info_t info{};
      int count = mp3dec_decode_frame(&decoder, input.data() + offset, int(input.size() - offset), output, &info);
      if (!info.frame_bytes) break;
      offset += info.frame_bytes;
      if (!count) continue;
      assert(info.layer == 3 && info.channels == (mono ? 1 : 2) && info.hz == (mono ? 22050 : 44100));
      for (int i = 0; i < count * info.channels; ++i) energy += double(output[i]) * output[i];
      samples += count; ++frames;
    }
    assert(frames > 10 && samples >= (mono ? 22050 : 44100) / 2 && energy > 1e8);
  }
  puts("PASS: vendored MP3-only decoder produces real PCM for stereo 44.1-kHz and mono 22.05-kHz fixtures");
}
