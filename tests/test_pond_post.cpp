#include "PondMultiplayer.h"
#include "AccentColor.h"
#include <cassert>
#include <iostream>

using namespace leap;

int main() {
  LocalPondRace local;
  local.start(UINT32_MAX - 5, 0);
  local.frame.positions = {10, 11, 0, 0};
  local.press(UINT32_MAX - 1, 0);
  assert(local.frame.phase == PondPhase::Rolling);
  assert(local.frame.dice == (PondDice{PondNone, PondNone}));
  local.press(1, 15); // Stop timing and a second random value cannot change the result.
  assert(local.frame.dice == (PondDice{0, 0}) && local.frame.positions[0] == 12);
  assert(local.frame.previous[0] == 10 && local.frame.winners == 1);
  local.press(2, 0);
  assert(local.frame.move == 1);
  local.tick(1 + PondSettleMs);
  assert(local.frame.turn == 1 && local.frame.phase == PondPhase::Ready);
  assert(local.botReady(1 + PondSettleMs + 570));
  local.roll(1800, 1);
  local.tick(2230);
  assert(local.frame.dice == (PondDice{1, 0}) && local.frame.winners == 3);
  local.tick(2230 + PondSettleMs);
  assert(local.frame.turn == 2);
  local.roll(4000, 15);
  local.tick(4430);
  assert(local.frame.positions == (PondPositions{12, 12, 0, 2}));
  local.tick(4430 + PondSettleMs);
  assert(local.frame.phase == PondPhase::Finished && local.frame.round == 1);

  JsonDocument doc;
  assert(!deserializeJson(doc, R"({"type":"state","schemaVersion":1,"session":"0123456789abcdef0123456789abcdef",
    "revision":7,"phase":"settling","self":1,"host":0,"turn":1,"remainingMs":1100,"move":2,"round":1,"turnId":2,
    "positions":[0,0,2,2],"previous":[0,0,0,2],"dice":[2,2],"winners":0,"reason":"","ack":3,
    "frogs":[{"name":"Änne","color":"#000000","human":true,"connected":true},
             {"name":"Mischa","color":"#000000","human":true,"connected":true},
             {"name":"Iris","color":"#9e92ee","human":false,"connected":false},
             {"name":"Mango","color":"#ffbd61","human":false,"connected":false}]})"));
  PondFrame frame;
  assert(decodePondFrame(doc, frame));
  assert(frame.frogs[0].color == 0 && frame.frogs[1].color == frame.frogs[0].color);
  assert(std::string(frame.frogs[0].name) == "Änne" && frame.ack == 3 && frame.dice[0] == 2);
  doc["dice"][1] = 4;
  assert(!decodePondFrame(doc, frame));
  doc["dice"][1] = 2;
  doc["positions"][0] = 13;
  assert(!decodePondFrame(doc, frame));
  doc["positions"][0] = 0;
  doc["previous"][0] = 1;
  assert(!decodePondFrame(doc, frame));
  assert(frame.phase == PondPhase::Settling && frame.positions[0] == 0);
  std::cout << "PASS: Pond race, two dice, final round, timing wrap and server snapshot bounds\n";
}
