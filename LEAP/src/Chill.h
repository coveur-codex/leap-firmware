#pragma once
#include "ChillMotion.h"
#include "Media.h"
#include <memory>
#include <vector>
namespace leap {
class Chill {
  struct Frame {
    String path, blob;
    std::unique_ptr<Media> media;
  };
  struct Sprite {
    Sprite() = default;
    Sprite(const Sprite &) = delete;
    Sprite &operator=(const Sprite &) = delete;
    Sprite(Sprite &&) noexcept = default;
    Sprite &operator=(Sprite &&) noexcept = default;
    String role;
    int x, y, width, height;
    std::vector<Frame> frames;
  };
  std::vector<Sprite> sprites;
  Preferences prefs;
  bool dirty = false;
  uint32_t changedAt = 0;
  String package;
  int version = 0;
  const char *sceneKey() const;
  void save();
  void sprite(Arduino_GFX &gfx, Sprite &s, int x, int y, int width, int height, size_t frame = 0);

public:
  ChillMotion motion;
  void begin() { prefs.begin("leap-chill", false); }
  bool active() const { return motion.scene != ChillScene::None; }
  bool start(const String &id, int nextVersion, JsonVariantConst manifest, uint32_t now);
  void close();
  void input(int delta, uint32_t now);
  void tick(uint32_t now);
  void draw(Arduino_GFX &gfx, uint32_t now);
};
} // namespace leap
