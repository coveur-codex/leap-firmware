#include "Chill.h"
#include "ChillAssets.h"
namespace leap {
static constexpr uint16_t SpaceBackground = 0x0001, FireBackground = 0x0800,
                          SnowBackground = 0x0885;
const char *Chill::sceneKey() const {
  switch (motion.scene) {
  case ChillScene::Space:
    return "space";
  case ChillScene::Fire:
    return "fire";
  case ChillScene::Snow:
    return "snow";
  default:
    return "none";
  }
}
void Chill::save() {
  if (dirty && active())
    prefs.putUChar(sceneKey(), motion.value);
  dirty = false;
}
void Chill::close() {
  save();
  sprites.clear();
  package = "";
  version = 0;
  motion.scene = ChillScene::None;
}
bool Chill::start(const String &id, int nextVersion, JsonVariantConst manifest, uint32_t now) {
  if (active() && package == id && version == nextVersion)
    return true;
  close();
  auto def = manifest["definition"];
  String scene = def["scene"] | "";
  ChillScene kind = scene == "space"  ? ChillScene::Space
                    : scene == "fire" ? ChillScene::Fire
                    : scene == "snow" ? ChillScene::Snow
                                      : ChillScene::None;
  if (kind == ChillScene::None || !nextVersion || !validChill(def, manifest["files"]))
    return false;
  motion.reset(kind, def["slider"]["default"] | 45, now);
  motion.value = std::min(100, int(prefs.getUChar(sceneKey(), motion.value)));
  package = id;
  version = nextVersion;
  // Resolve from the immutable in-RAM manifest once. Decode only via the existing media worker.
  auto addFrame = [&](Sprite &s, String path) {
    for (JsonObjectConst f : manifest["files"].as<JsonArrayConst>())
      if (f["path"] == path) {
        s.frames.push_back(
            {path, storage.blob(f["sha256"].as<String>()), std::unique_ptr<Media>(new Media())});
        return;
      }
  };
  for (JsonObjectConst item : def["sprites"].as<JsonArrayConst>()) {
    if (sprites.size() >= 16)
      break;
    Sprite s;
    s.role = item["role"] | "decor";
    s.width = std::clamp(item["size"][0] | 64, 1, 142);
    s.height = std::clamp(item["size"][1] | 64, 1, 142);
    s.x = item["x"] | ((428 - s.width) / 2);
    s.y = item["y"] | (142 - s.height);
    if (item["file"].is<const char *>())
      addFrame(s, item["file"].as<String>());
    for (JsonVariantConst f : item["frames"].as<JsonArrayConst>()) {
      if (s.frames.size() >= 8)
        break;
      addFrame(s, f.as<String>());
    }
    if (!s.frames.empty())
      sprites.push_back(std::move(s));
  }
  return true;
}
bool Chill::input(const InputEvent &event, uint32_t now) {
  if (event.longPress)
    return false;
  // Both physical centre buttons mean Back; never route them to the slider.
  if (event.key == Key::Center) {
    close();
    return true;
  }
  if (!active() || !event.right)
    return false;
  int delta = (event.key == Key::Right || event.key == Key::Up) -
              (event.key == Key::Left || event.key == Key::Down);
  if (motion.input(delta * 5, now)) {
    dirty = true;
    changedAt = now;
  }
  return false;
}
void Chill::tick(uint32_t now) {
  if (dirty && uint32_t(now - changedAt) >= 2000)
    save();
}
void Chill::sprite(Arduino_GFX &gfx, Sprite &s, int x, int y, int width, int height, size_t frame) {
  if (s.frames.empty())
    return;
  auto &f = s.frames[frame % s.frames.size()];
  uint16_t bg = motion.scene == ChillScene::Fire   ? FireBackground
                : motion.scene == ChillScene::Snow ? SnowBackground
                                                   : SpaceBackground;
  f.media->draw(gfx, f.blob, f.path, x, y, width, height, false, bg, true, false, true);
}
void Chill::draw(Arduino_GFX &gfx, uint32_t now) {
  motion.step(now);
  auto scene = motion.scene;
  gfx.fillScreen(scene == ChillScene::Fire   ? FireBackground
                 : scene == ChillScene::Snow ? SnowBackground
                                             : SpaceBackground);
  if (scene == ChillScene::Snow) {
    for (auto &s : sprites)
      sprite(gfx, s, s.x, s.y, s.role == "foreground" ? 428 : s.width, s.height);
  } else if (scene == ChillScene::Fire) {
    // Low, slowly breathing light, behind the transparent wood/ember/flame assets.
    int glow = 12 + motion.value / 5 + int(2 * std::sin(motion.time));
    gfx.fillCircle(214, 104, glow, 0x1800 + ((motion.value / 20) << 5));
    for (auto &s : sprites) {
      if (s.role == "animation") {
        int h = s.height * (55 + motion.value * 45 / 100) / 100;
        int w = s.width * (55 + motion.value * 45 / 100) / 100;
        sprite(gfx, s, s.x + (s.width - w) / 2, s.y + s.height - h, w, h,
               size_t(motion.time * (4 + motion.value * .06f)));
      } else
        sprite(gfx, s, s.x, s.y, s.width, s.height);
    }
  } else if (scene == ChillScene::Space && !sprites.empty()) {
    // At most one decoration, then eight seconds of uninterrupted stars.
    float phase = std::fmod(motion.time, 40.f);
    if (phase >= 8) {
      auto &s = sprites[size_t(motion.time / 40) % sprites.size()];
      float progress = (phase - 8) / 32;
      sprite(gfx, s, int(428 - progress * (428 + s.width)), 18, s.width, s.height);
    }
  }
  for (int i = 0; i < motion.count(); ++i) {
    auto &p = motion.particles[i];
    uint16_t color = scene == ChillScene::Fire ? (p.y > 75 ? 0xfdc0 : 0x92a0)
                     : p.depth > .7f           ? 0xef7d
                                               : 0x8c71;
    if (p.depth > .8f && scene != ChillScene::Fire)
      gfx.fillCircle(int(p.x), int(p.y), 1, color);
    else
      gfx.drawPixel(int(p.x), int(p.y), color);
  }
  // A small back chevron is the only persistent UI element.
  gfx.drawLine(12, 8, 6, 14, 0x8c71);
  gfx.drawLine(6, 14, 12, 20, 0x8c71);
  if (motion.sliderVisible(now)) {
    gfx.fillRoundRect(122, 116, 184, 20, 5, 0x18e3);
    gfx.drawFastHLine(138, 126, 152, 0x8c71);
    gfx.fillCircle(138 + motion.value * 152 / 100, 126, 4, 0xce79);
  }
}
} // namespace leap
