#include "Audio.h"
#include "Games.h"
#include "Motion.h"
#include <cassert>
#include <iostream>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n) {
  return malloc(n);
}
void RamAllocator::deallocate(void *p) {
  free(p);
}
void *RamAllocator::reallocate(void *p, size_t n) {
  return realloc(p, n);
}
Storage storage;
Audio audio;
Motion motion;
void Audio::tone(uint16_t, uint16_t) {}
int Motion::direction(bool) {
  return -1;
}
bool Motion::shake() {
  return false;
}
Media::~Media() {}
// Record the actual rendering request, avoiding any fake image decoding claim.
std::vector<std::string> drawn;
bool Media::draw(Arduino_GFX &, const String &, const String &path, int, int, int, int, bool,
                 uint16_t, bool, bool, bool) {
  drawn.push_back(path);
  return true;
}
} // namespace leap
static PetState savedPet() {
  PetState p;
  auto &data = Preferences::bytes["leap-gamespet"];
  assert(data.size() == sizeof(p));
  memcpy(&p, data.data(), sizeof(p));
  return p;
}
int main() {
  Games game;
  game.begin();
  Arduino_GFX gfx;
  game.start("tamagotchi");
  assert(game.active());
  fakeNow = 3600000;
  game.tick();
  assert(savedPet().food == 83);
  game.input(Key::Center);
  assert(savedPet().food == 100);
  JsonDocument manifest;
  auto idle = manifest["definition"]["tamagotchi"]["animations"]["idle"]["frames"].to<JsonArray>();
  idle.add("idle.png");
  auto happy =
      manifest["definition"]["tamagotchi"]["animations"]["happy"]["frames"].to<JsonArray>();
  happy.add("happy.png");
  auto eating =
      manifest["definition"]["tamagotchi"]["animations"]["eating"]["frames"].to<JsonArray>();
  for (int i = 0; i < 4; ++i)
    eating.add("eating" + std::to_string(i) + ".png");
  game.avatarPackage("avatar-test", 2, manifest);
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "eating0.png");
  fakeNow += 400;
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "eating1.png");
  fakeNow += 400;
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "eating2.png");
  fakeNow += 400;
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "eating3.png");
  fakeNow += 2000;
  game.tick();
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "happy.png");
  game.close();
  assert(!game.active());
  Games reboot;
  reboot.begin();
  reboot.start("tamagotchi");
  fakeNow += 3600000;
  reboot.tick();
  assert(savedPet().food == 98); // Restored 100, not defaults.
  reboot.input(Key::Down);
  reboot.input(Key::Center);
  assert(savedPet().joy == 100);
  reboot.input(Key::Down);
  reboot.input(Key::Center);
  assert(savedPet().clean == 100);
  reboot.input(Key::Down);
  reboot.input(Key::Center);
  assert(savedPet().energy == 100);
  reboot.close();
  PetState corrupt;
  corrupt.version = 0;
  auto p = reinterpret_cast<const uint8_t *>(&corrupt);
  Preferences::bytes["leap-gamespet"] = {p, p + sizeof(corrupt)};
  Games recovered;
  recovered.begin();
  fakeNow += 3600000;
  recovered.tick();
  assert(savedPet().food == 83);
  recovered.start("snake");
  assert(recovered.active());
  // Food index 258 corresponds to the square directly ahead of the initial head.
  fakeRandom = 258;
  recovered.start("snake");
  fakeNow += 220;
  recovered.tick();
  assert(Preferences::ints["leap-gamessnake-best"] == 1);
  recovered.close();
  fakeNow += 5000;
  recovered.tick();
  assert(!recovered.active());
  Games newSession;
  newSession.begin();
  newSession.start("snake");
  gfx.text.clear();
  newSession.draw(gfx, 94, 10);
  assert(gfx.text.find("Rekord 1") != std::string::npos);
  for (int i = 0; i < 40; ++i) {
    fakeNow += 220;
    newSession.tick();
  }
  assert(!newSession.active());
  newSession.start("snake");
  assert(newSession.active());
  newSession.close();
  assert(!newSession.active());
  std::cout << "PASS: real Games actions, four animation frames, timed reset, Preferences "
               "restart/corruption, Snake exit and highscore\n";
}
