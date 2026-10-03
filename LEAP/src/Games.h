#pragma once
#include "GameRules.h"
#include "Input.h"
#include "Media.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class Games {
  String kind, avatarId;
  int avatarVersion = 0, petSelection = 0;
  JsonDocument petManifest{&jsonRam};
  String petBlob(const String &path) const;
  Media petImage, petBackground;
  PetState pet;
  SnakeState snake;
  Preferences gamePrefs;
  uint32_t petLast = 0, actionAt = 0;
  int petAction = -1, highscore = 0;
  bool opened = false;
  void savePet();
  void drawPet(Arduino_GFX &gfx);
  uint32_t started = 0, last = 0;
  int score = 0, sequence[32]{}, length = 1, step = 0, show = 0, x = 0, y = 0;
  bool running = false, showing = false, won = false;

public:
  void begin();
  void avatarPackage(const String &id, int version, JsonVariantConst manifest);
  void start(const String &id);
  void close();
  bool isPet() const {
    return kind == "tamagotchi";
  }
  bool isSnake() const {
    return kind == "snake";
  }
  void input(Key key);
  void tick();
  void draw(Arduino_GFX &gfx, int left, int top);
  bool active() const {
    return running;
  }
};
} // namespace leap
