#include "Media.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace leap;
int main(int argc, char **argv) {
  assert(argc > 1);
  for (int i = 1; i < argc; ++i) {
    std::ifstream in(argv[i], std::ios::binary);
    unsigned char header[24];
    in.read(reinterpret_cast<char *>(header), 24);
    assert(in);
    auto dimension = [&](int offset) {
      return (header[offset] << 24) | (header[offset + 1] << 16) | (header[offset + 2] << 8) |
             header[offset + 3];
    };
    int w = dimension(16), h = dimension(20);
    assert(w >= 1 && w <= 142 && h >= 1 && h <= 142);
    assert(Media::validate(argv[i], "sprite.png"));
    uint8_t *mask = nullptr;
    auto pixels = Media::decode(argv[i], "sprite.png", w, h, nullptr, nullptr, 0x0800, &mask);
    assert(pixels && mask);
    int opaque = 0, transparent = 0;
    for (int n = 0; n < w * h; ++n) {
      if (mask[n])
        ++opaque;
      else
        ++transparent;
    }
    assert(opaque && transparent);
    free(pixels);
    free(mask);
  }
  std::cout
      << "PASS: all " << argc - 1
      << " actual Chill PNG sprites/frames decoded with alpha by Media.cpp and pinned PNGdec\n";
}
