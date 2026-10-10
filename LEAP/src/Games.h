#pragma once
#include "GameRules.h"
#include "KitchenEditor.h"
#include "ConnectFour.h"
#include "Maze.h"
#include "CrabJourney.h"
#include "CrabJourneyProgress.h"
#include "DragonRun.h"
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
  DragonRun dragon;
  int dragonBest = 0;
  bool dragonDirty = false, dragonJumpHeld = false;
  uint32_t dragonSavedAt = 0;
  void saveDragon();
  CrabJourney crab;
  uint8_t crabDirections = 0;
  CrabJourneyProgress crabProgress;
  bool crabDirty = false, crabResetOpen = false, crabResetYes = false;
  uint32_t crabSavedAt = 0;
  void saveCrab();
  void checkpointCrab(uint32_t level);
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
  bool isDragonRun() const {
    return kind == "dragon_run";
  }
  bool dragonInput(const InputEvent &event);
  bool isCrabJourney() const {
    return kind == "crab_journey";
  }
  void heldDirections(uint8_t mask) {
    crabDirections = mask;
    if (!(mask & 1))
      dragonJumpHeld = false;
  }
  bool isKitchen() const {
    return kind == "kitchen";
  }
  bool kitchenInput(const InputEvent &event);
  bool crabInput(const InputEvent &event);
  void input(Key key);
  void tick();
  void draw(Arduino_GFX &gfx, int left, int top);
  bool active() const {
    return running;
  }
};
} // namespace leap
