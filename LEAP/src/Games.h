#pragma once
#include "GameRules.h"
#include "SnakeMultiplayer.h"
#include "MathLab.h"
#include "KitchenEditor.h"
#include "ConnectFour.h"
#include "FourMultiplayer.h"
#include "Maze.h"
#include "CrabJourney.h"
#include "CrabJourneyProgress.h"
#include "DragonRun.h"
#include "FallBlocks.h"
#include "Rangierwerk.h"
#include "PondPost.h"
#include "DiceGame.h"
#include "Minesweeper.h"
#include "Breakout.h"
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
  bool snakeCoop = false;
  SnakeSnapshot cooperative;
  JsonDocument gameAvatars{&jsonRam};
  Media lobbyImages[4];
  void drawCooperative(Arduino_GFX &gfx, int left, int top);
  MazeState maze;
  DragonRun dragon;
  int dragonBest = 0;
  bool dragonDirty = false, dragonJumpHeld = false;
  uint32_t dragonSavedAt = 0;
  void saveDragon();
  FallBlocks fallBlocks;
  Rangierwerk rangierwerk;
  PondPost pondPost;
  DiceGame diceGame;
  Minesweeper minesweeper;
  Breakout breakout;
  void drawMinesweeper(Arduino_GFX &gfx, int left, int top, uint16_t accent) const;
  void drawBreakout(Arduino_GFX &gfx, int left, int top, uint16_t accent) const;
  int fallBlocksBest = 0;
  bool fallBlocksDirty = false, fallBlocksSoftDrop = false;
  uint32_t fallBlocksSavedAt = 0;
  void saveFallBlocks();
  void updateFallBlocksRound();
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
  bool fourOnline = false, fourSearchExpired = false;
  FourSnapshot versus;
  int fourPlayer() const;
  void drawFourOnline(Arduino_GFX &gfx, int left, int top);
  int fourDifficulty = 0, fourColumn = 3;
  bool fourSetup = true, fourThinking = false, fourFull = false;
  void drawFour(Arduino_GFX &gfx, int left, int top);
  MathLab mathLab;
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
  bool takeMultiplayerTimeout() { bool result=fourSearchExpired; fourSearchExpired=false; return result; }
  void multiplayerAssets(JsonVariantConst manifest) { gameAvatars.set(manifest); }
  void avatarPackage(const String &id, int version, JsonVariantConst manifest);
  void start(const String &id);
  void close();
  bool isPet() const {
    return kind == "tamagotchi";
  }
  bool isSnakeMultiplayer() const { return isSnake() && snakeCoop; }
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
  bool isPondPost() const { return kind == "pfuetzenpost"; }
  bool pondAnimating() const { return pondPost.animating(); }
  bool isDiceGame() const { return kind == "dice"; }
  bool diceAnimating() const { return opened && isDiceGame() && diceGame.animating(); }
  bool diceInput(const InputEvent &event) { return opened && isDiceGame() && diceGame.input(event, millis()); }
  bool isRangierwerk() const { return kind == "rangierwerk"; }
  bool rangierwerkInput(const InputEvent &event) { return rangierwerk.input(event); }
  bool rangierwerkAwake() const { return rangierwerk.keepAwake(); }
  bool isFallBlocks() const { return kind == "fall_blocks"; }
  bool isMinesweeper() const { return kind == "minesweeper"; }
  bool isBreakout() const { return kind == "breakout"; }
  bool isLocalArcade() const { return isMinesweeper() || isBreakout(); }
  void arcadeInput(const InputEvent &event);
  const char *arcadeHint() const;
  void suspendBreakout() {
    if (opened && isBreakout())
      breakout.suspend();
  }
  bool fallBlocksInput(const InputEvent &event);
  void drawFallBlocks(Arduino_GFX &gfx, uint16_t accent);
  bool isCrabJourney() const {
    return kind == "crab_journey";
  }
  void heldDirections(uint8_t mask) {
    crabDirections = mask;
    if (isRangierwerk())
      rangierwerk.directions(mask);
    fallBlocksSoftDrop = mask & (1u << int(Key::Right));
    if (!(mask & 1))
      dragonJumpHeld = false;
  }
  bool isKitchen() const {
    return kind == "kitchen";
  }
  bool isMathLab() const {
    return kind == "math_lab";
  }
  void mathLabTargets(JsonVariantConst config) {
    auto values = config["targets"].as<JsonArrayConst>();
    mathLab.targetCount = 0;
    for (auto value : values) {
      if (value.is<int>() && value.as<int>() >= 1 && value.as<int>() <= 100 && mathLab.targetCount < 100)
        mathLab.targets[mathLab.targetCount++] = value.as<int>();
    }
    if (!mathLab.targetCount) { mathLab.targets[0]=10; mathLab.targetCount=1; }
  }
  bool mathLabInput(const InputEvent &event) {
    return mathLab.input(event);
  }
  bool kitchenInput(const InputEvent &event);
  bool crabInput(const InputEvent &event);
  void input(Key key);
  void tick();
  void draw(Arduino_GFX &gfx, int left, int top, uint16_t accent = 0x06b8);
  bool active() const {
    return running;
  }
};
} // namespace leap
