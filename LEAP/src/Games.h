#pragma once
#include "GameRules.h"
#include "KitchenEditor.h"
#include "ConnectFour.h"
#include "Maze.h"
#include "Input.h"
#include "Media.h"
#include <Arduino_GFX_Library.h>
namespace leap {
class Games {
  String kind, avatarId;
  int avatarVersion = 0, petSelection = 0;
  JsonDocument petManifest{&jsonRam}, petAssets{&jsonRam};
  String petBlob(const String &path) const;
  Media petImage, petBackground;
  PetState pet;
  SnakeState snake;
  MazeState maze;
  int snakeSpeed = 0;
  bool snakeSetup = true;
  inline static constexpr uint32_t snakeIntervals[] = {450, 300, 220};
  ConnectFour four;
  int fourDifficulty = 0, fourColumn = 3;
  bool fourSetup = true, fourThinking = false, fourFull = false;
  void drawFour(Arduino_GFX &gfx, int left, int top);
  KitchenEditor kitchen;
  bool kitchenDirty = false;
  uint32_t kitchenSavedAt = 0;
  void saveKitchen();
  Preferences gamePrefs;
  uint32_t petLast = 0, petSavedAt = 0, actionAt = 0;
  bool petDirty = false;
  int petAction = -1, highscore = 0;
  bool opened = false;
  void savePet();
  void drawPet(Arduino_GFX &gfx);
  uint32_t started = 0, last = 0;
  int score = 0, sequence[32]{}, length = 1, step = 0, show = 0;
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
  bool isConnectFour() const {
    return kind == "connect_four";
  }
  bool isKitchen() const {
    return kind == "kitchen";
  }
  bool kitchenInput(const InputEvent &event);
  void input(Key key);
  void tick();
  void draw(Arduino_GFX &gfx, int left, int top);
  bool active() const {
    return running;
  }
};
} // namespace leap
