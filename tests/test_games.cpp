#include "GameRules.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace leap;
int main() {
  PetState pet;
  assert(pet.valid() && std::strcmp(pet.mood(), "happy") == 0);
  pet.decay(5);
  assert(pet.food == 75 && pet.energy == 75 && pet.joy == 80);
  assert(std::strcmp(pet.mood(), "idle") == 0);
  pet.decay(1000000);
  assert(pet.valid() && pet.food >= 35);
  assert(std::strcmp(pet.mood(), "hungry") == 0);
  pet.care(0);
  assert(std::strcmp(pet.mood(), "tired") == 0);
  pet.care(3);
  assert(std::strcmp(pet.mood(), "idle") == 0);
  pet.clean = 40;
  assert(std::strcmp(pet.mood(), "dirty") == 0);
  pet.care(2);
  pet.joy = 40;
  assert(std::strcmp(pet.mood(), "sad") == 0);
  pet.care(1);
  assert(pet.valid());
  PetState reboot;
  std::memcpy(&reboot, &pet, sizeof(pet));
  assert(reboot.valid() && reboot.food == pet.food && reboot.energy == pet.energy);
  reboot.version = 2;
  assert(!reboot.valid());
  reboot.version = 1;
  reboot.joy = 101;
  assert(!reboot.valid());
  assert(!petNight(false, 22));
  assert(petNight(true, 0));
  assert(petNight(true, 6));
  assert(!petNight(true, 7));
  assert(!petNight(true, 19));
  assert(petNight(true, 20));
  SnakeState snake;
  snake.start(0);
  assert(snake.alive && snake.length == 3 && snake.score == 0);
  for (int i = 0; i < snake.length; ++i)
    assert(!(snake.food == snake.body[i]));
  snake.turn(2);
  assert(!snake.turned); // reverse ignored
  snake.turn(0);
  snake.turn(2);
  assert(snake.queued == 0); // one turn per move
  snake.move(1);
  assert(snake.body[0].y == SnakeState::Rows / 2 - 1);
  snake.start(0);
  snake.food = {snake.body[0].x + 1, snake.body[0].y};
  assert(snake.move(123) && snake.length == 4 && snake.score == 1);
  for (int i = 0; i < snake.length; ++i)
    assert(!(snake.food == snake.body[i]));
  snake.body[0] = {SnakeState::Columns - 1, 0};
  snake.move(0);
  assert(!snake.alive);
  snake.start(0);
  assert(snake.alive && snake.score == 0 && snake.length == 3);
  snake.length = 5;
  snake.body[0] = {2, 2};
  snake.body[1] = {2, 3};
  snake.body[2] = {3, 3};
  snake.body[3] = {3, 2};
  snake.body[4] = {4, 2};
  snake.food = {0, 0};
  snake.move(0);
  assert(!snake.alive); // own body
  snake.start(0);
  snake.length = 4;
  snake.body[0] = {2, 2};
  snake.body[1] = {2, 3};
  snake.body[2] = {3, 3};
  snake.body[3] = {3, 2};
  snake.food = {0, 0};
  snake.move(0);
  assert(snake.alive); // departing tail
  snake.length = SnakeState::Capacity;
  for (int i = 0; i < snake.length; ++i)
    snake.body[i] = {i % SnakeState::Columns, i / SnakeState::Columns};
  snake.placeFood(0);
  assert(!snake.alive && snake.won);
  std::cout
      << "PASS: pet needs, persistence data, day/night, Snake movement, food and collisions\n";
}
