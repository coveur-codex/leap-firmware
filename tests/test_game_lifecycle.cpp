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
struct DrawRequest {
  String blob, path;
  int x, y, w, h;
  bool transparent;
};
std::vector<DrawRequest> requests;
bool imagesAvailable = true;
bool Media::draw(Arduino_GFX &, const String &blob, const String &path, int x, int y, int w, int h,
                 bool, uint16_t, bool, bool, bool transparent) {
  drawn.push_back(path);
  requests.push_back({blob, path, x, y, w, h, transparent});
  return imagesAvailable;
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
  Arduino_GFX gfx;
  Games kitchenGame;
  kitchenGame.begin();
  kitchenGame.start("kitchen");
  assert(kitchenGame.active() && kitchenGame.isKitchen());
  assert(Preferences::bytes["leap-gameskitchen"].empty());
  kitchenGame.kitchenInput({true, Key::Center, false});
  auto savedKitchen = Preferences::bytes["leap-gameskitchen"];
  KitchenState loadedKitchen;
  assert(loadedKitchen.decode(savedKitchen.data(), savedKitchen.size()));
  assert(loadedKitchen.count == 1 && loadedKitchen.objects[0].type == KitchenType::Cabinet);
  kitchenGame.kitchenInput({false, Key::Center, false});
  kitchenGame.kitchenInput({false, Key::Right, false});
  kitchenGame.close(); // An unconfirmed move must never persist.
  assert(Preferences::bytes["leap-gameskitchen"] == savedKitchen);
  Games kitchenReboot;
  kitchenReboot.begin();
  kitchenReboot.start("kitchen");
  kitchenReboot.kitchenInput({true, Key::Right, false}); // Replace cabinet with drawers.
  kitchenReboot.kitchenInput({true, Key::Center, false});
  savedKitchen = Preferences::bytes["leap-gameskitchen"];
  assert(loadedKitchen.decode(savedKitchen.data(), savedKitchen.size()));
  assert(loadedKitchen.count == 1 && loadedKitchen.objects[0].type == KitchenType::Drawers);
  kitchenReboot.close();
  Preferences::failWrites = true;
  kitchenReboot.start("kitchen");
  kitchenReboot.kitchenInput({false, Key::Right, false});
  kitchenReboot.kitchenInput({true, Key::Center, false});
  assert(Preferences::bytes["leap-gameskitchen"] == savedKitchen);
  gfx.text.clear();
  kitchenReboot.draw(gfx, 94, 10);
  assert(gfx.text.find("Speichern...") != std::string::npos);
  Preferences::failWrites = false;
  fakeNow += 5000;
  kitchenReboot.tick();
  savedKitchen = Preferences::bytes["leap-gameskitchen"];
  assert(loadedKitchen.decode(savedKitchen.data(), savedKitchen.size()));
  assert(loadedKitchen.count == 2);
  kitchenReboot.close();
  kitchenReboot.start("kitchen");
  assert(!kitchenReboot.kitchenInput({false, Key::Center, false}));
  assert(!kitchenReboot.kitchenInput({false, Key::Center, true, 900}));
  assert(kitchenReboot.kitchenInput({false, Key::Center, true, 2000}));
  kitchenReboot.close();
  assert(Preferences::bytes["leap-gameskitchen"] == savedKitchen);
  kitchenReboot.start("kitchen");
  kitchenReboot.kitchenInput({true, Key::Left, false}); // Kueche leeren.
  kitchenReboot.kitchenInput({true, Key::Center, false});
  assert(Preferences::bytes["leap-gameskitchen"] == savedKitchen);
  kitchenReboot.kitchenInput({true, Key::Down, false});
  kitchenReboot.kitchenInput({true, Key::Center, false});
  savedKitchen = Preferences::bytes["leap-gameskitchen"];
  assert(loadedKitchen.decode(savedKitchen.data(), savedKitchen.size()));
  assert(loadedKitchen.count == 0);
  kitchenReboot.close();
  Games emptyReboot;
  emptyReboot.begin(); emptyReboot.start("kitchen");
  emptyReboot.kitchenInput({true, Key::Left, false}); // Reset again.
  emptyReboot.kitchenInput({true, Key::Center, false});
  emptyReboot.kitchenInput({true, Key::Down, false});
  emptyReboot.kitchenInput({true, Key::Center, false});
  assert(Preferences::bytes["leap-gameskitchen"] == savedKitchen);
  emptyReboot.kitchenInput({true, Key::Left, false});
  emptyReboot.kitchenInput({true, Key::Left, false}); // Zurueck.
  assert(emptyReboot.kitchenInput({true, Key::Center, false}));
  emptyReboot.close();
  fakeNow = 0; // Preserve the existing pet timing regression below.
  Games game;
  game.begin();
  game.start("tamagotchi");
  assert(game.active());
  fakeNow = PetState::DecayIntervalMs;
  game.tick();
  game.close(); // Leaving saves even a partial persistence interval.
  assert(savedPet().food == 83);
  game.start("tamagotchi");
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
  JsonDocument legacy;
  legacy["definition"]["preview"] = "preview.svg";
  legacy["files"].to<JsonArray>().add<JsonObject>()["path"] = "data/pet/idle/frame_01.png";
  game.avatarPackage("avatar-legacy", 3, legacy);
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "data/pet/idle/frame_01.png");
  JsonDocument oldUpload;
  auto oldFiles = oldUpload["files"].to<JsonArray>();
  for (int i = 4; i > 0; --i) {
    auto file = oldFiles.add<JsonObject>();
    file["path"] = "Example/data/pet/idle/frame_0" + std::to_string(i) + ".png";
    file["sha256"] = std::string(64, 'a');
  }
  for (const char *period : {"day", "night"}) {
    auto file = oldFiles.add<JsonObject>();
    file["path"] = "background_" + std::string(period) + ".png";
    file["sha256"] = std::string(64, 'b');
  }
  game.avatarPackage("avatar-old-upload", 4, oldUpload);
  drawn.clear();
  fakeNow = ((fakeNow + 1599) / 1600) * 1600;
  game.draw(gfx, 94, 10);
  assert(drawn.size() == 2);
  assert(drawn[0] == "background_day.png" || drawn[0] == "background_night.png");
  assert(drawn[1] == "Example/data/pet/idle/frame_01.png");
  fakeNow += 400;
  game.draw(gfx, 94, 10);
  assert(drawn.back() == "Example/data/pet/idle/frame_02.png");
  game.close();
  assert(!game.active());
  Games reboot;
  reboot.begin();
  reboot.start("tamagotchi");
  fakeNow += PetState::DecayIntervalMs;
  reboot.tick();
  reboot.close();
  reboot.start("tamagotchi");
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
  recovered.start("tamagotchi");
  fakeNow += PetState::DecayIntervalMs;
  recovered.tick();
  recovered.close();
  assert(savedPet().food == 83);
  recovered.start("snake");
  assert(recovered.active());
  gfx.text.clear();
  recovered.draw(gfx, 94, 10);
  assert(gfx.text.find("Geschwindigkeit") != std::string::npos);
  // Food index 258 corresponds to the square directly ahead of the initial head.
  recovered.start("snake");
  fakeRandom = 258;
  recovered.input(Key::Center);
  fakeNow += 450;
  recovered.tick();
  assert(Preferences::ints["leap-gamessnake-best"] == 1);
  recovered.close();
  fakeNow += 5000;
  recovered.tick();
  assert(!recovered.active());
  Games newSession;
  newSession.begin();
  newSession.start("snake");
  newSession.input(Key::Center);
  gfx.text.clear();
  newSession.draw(gfx, 94, 10);
  assert(gfx.text.find("Rekord 1") != std::string::npos);
  for (int i = 0; i < 40; ++i) {
    fakeNow += 450;
    newSession.tick();
  }
  assert(!newSession.active());
  newSession.start("snake");
  assert(newSession.active());
  newSession.close();
  assert(!newSession.active());
  for (int speed = 0; speed < 3; ++speed) {
    Games paced;
    paced.begin();
    paced.start("snake");
    fakeNow += 5000;
    paced.tick();
    gfx.text.clear();
    paced.draw(gfx, 94, 10);
    assert(gfx.text.find("Geschwindigkeit") != std::string::npos);
    for (int i = 0; i < speed; ++i) paced.input(Key::Down);
    paced.input(Key::Center);
    auto headX = [&] {
      gfx.rects.clear();
      paced.draw(gfx, 94, 10);
      for (auto r : gfx.rects) if (r.color == 0x07ff && r.w == 7) return r.x;
      return -1;
    };
    int before = headX();
    const int intervals[] = {450, 300, 220};
    fakeNow += intervals[speed] - 1;
    paced.tick();
    assert(headX() == before);
    ++fakeNow;
    paced.tick();
    assert(headX() == before + 8);
    paced.close();
    fakeNow += 2000;
    paced.tick();
    assert(!paced.active());
  }
  // Exact v6 path layout and all nine states supplied by the dragon manifest.
  JsonDocument dragon;
  dragon["definition"]["tamagotchi"]["backgrounds"]["day"] = "data/background/background_day.png";
  dragon["definition"]["tamagotchi"]["backgrounds"]["night"] =
      "data/background/background_night.png";
  auto dragonFiles = dragon["files"].to<JsonArray>();
  for (const char *period : {"day", "night"}) {
    auto file = dragonFiles.add<JsonObject>();
    file["path"] = "data/background/background_" + std::string(period) + ".png";
    file["sha256"] = std::string(64, 'b');
  }
  for (const char *state :
       {"idle", "happy", "hungry", "tired", "dirty", "sad", "eating", "playing", "sleeping"}) {
    auto animation = dragon["definition"]["tamagotchi"]["animations"][state];
    animation["frameDurationMs"] = 400;
    auto frames = animation["frames"].to<JsonArray>();
    for (int frame = 1; frame <= 4; ++frame) {
      std::string path =
          "data/pet/" + std::string(state) + "/frame_0" + std::to_string(frame) + ".png";
      frames.add(path);
      auto file = dragonFiles.add<JsonObject>();
      file["path"] = path;
      file["sha256"] = std::string(64, 'a');
    }
  }
  auto installPet = [](PetState value) {
    auto ptr = reinterpret_cast<const uint8_t *>(&value);
    Preferences::bytes["leap-gamespet"] = {ptr, ptr + sizeof(value)};
  };
  auto render = [&](Games &petGame) {
    requests.clear();
    gfx.text.clear();
    gfx.rects.clear();
    petGame.draw(gfx, 94, 10);
    assert(requests.size() == 2);
    assert(requests[0].blob == storage.blob(String(std::string(64, 'b'))));
    assert(requests[0].x == 86 && requests[0].w == 256 && requests[0].h == 142);
    assert(requests[1].transparent);
    assert(gfx.text.find("SattSpassSauberKraft") != std::string::npos);
  };
  const char *moods[] = {"hungry", "sad", "dirty", "tired", "idle", "happy"};
  const char *messages[] = {"Hunger", "Spiel mit mir", "wasch mich", "muede", "gut", "gluecklich"};
  for (int i = 0; i < 6; ++i) {
    PetState needs;
    if (i == 0)
      needs.food = 40;
    if (i == 1)
      needs.joy = 40;
    if (i == 2)
      needs.clean = 40;
    if (i == 3)
      needs.energy = 40;
    if (i == 4)
      needs.food = 70;
    installPet(needs);
    Games petGame;
    petGame.begin();
    petGame.avatarPackage("avatar-dragon", 6, dragon);
    petGame.start("tamagotchi");
    render(petGame);
    assert(requests[1].path.find("/" + std::string(moods[i]) + "/") != std::string::npos);
    assert(gfx.text.find(messages[i]) != std::string::npos);
    if (i < 4) {
      bool warningBar = false;
      for (auto r : gfx.rects)
        warningBar |= r.x == 90 + i * 63 && r.y == 132 && r.w == 22 && r.color == 0xf9a0;
      assert(warningBar);
    }
  }
  installPet(PetState{});
  Games petGame;
  petGame.begin();
  petGame.avatarPackage("avatar-dragon", 6, dragon);
  petGame.start("tamagotchi");
  const char *actions[] = {"eating", "playing", "happy", "sleeping"};
  for (int action = 0; action < 4; ++action) {
    if (action)
      petGame.input(Key::Down);
    petGame.input(Key::Center);
    for (int frame = 1; frame <= 4; ++frame) {
      render(petGame);
      assert(requests[1].path == "data/pet/" + std::string(actions[action]) + "/frame_0" +
                                     std::to_string(frame) + ".png");
      if (action == 3)
        assert(requests[0].path == "data/background/background_night.png");
      fakeNow += 400;
    }
    fakeNow += 1400;
    petGame.tick();
    render(petGame);
    assert(requests[1].path.find("/happy/") != std::string::npos);
  }
  // Missing/failed images still produce a landscape and usable needs/actions.
  imagesAvailable = false;
  render(petGame);
  bool ground = false;
  for (auto r : gfx.rects)
    ground |= r.x == 86 && r.y == 113 && r.w == 256 && r.h == 29;
  assert(ground && gfx.text.find("Fuettern") != std::string::npos);
  imagesAvailable = true;
  Games four;
  four.begin();
  four.start("connect_four");
  auto fourText = [&]() {
    gfx.text.clear();
    four.draw(gfx, 94, 10);
    return gfx.text;
  };
  assert(four.active() && four.isConnectFour());
  assert(fourText().find("Schwierigkeit") != std::string::npos);
  four.input(Key::Down);
  four.input(Key::Down);
  four.input(Key::Down);
  four.input(Key::Center);
  assert(fourText().find("Stufe: Schwer") != std::string::npos);
  four.input(Key::Center);
  assert(fourText().find("LEAP denkt") != std::string::npos);
  four.input(Key::Center); // No second human move while the device is thinking.
  four.close();
  fakeNow += 500;
  four.tick();
  assert(!four.active());
  four.start("connect_four");
  four.input(Key::Center); // Easy.
  fakeRandom = 0; // Device always takes the first legal column.
  for (int i = 0; i < 3; ++i)
    four.input(Key::Right);
  for (int turn = 0; turn < 4; ++turn) {
    four.input(Key::Center);
    if (turn < 3) {
      four.input(Key::Center);
      fakeNow += 299;
      four.tick();
      assert(fourText().find("LEAP denkt") != std::string::npos);
      fakeNow += 1;
      fakeRandom = 0;
      four.tick();
      assert(fourText().find("Du bist dran") != std::string::npos);
    }
  }
  assert(!four.active() && fourText().find("Du gewinnst!") != std::string::npos);
  four.input(Key::Center);
  assert(four.active() && fourText().find("Schwierigkeit") != std::string::npos);
  four.input(Key::Center);
  // Alternate both players in the selected column until it is full.
  for (int turn = 0; turn < 3; ++turn) {
    four.input(Key::Center);
    fakeNow += 300;
    fakeRandom = 3;
    four.tick();
  }
  four.input(Key::Center);
  assert(fourText().find("Spalte voll!") != std::string::npos);
  fakeNow += 300;
  four.tick();
  assert(fourText().find("Spalte voll!") != std::string::npos);
  four.input(Key::Left);
  four.input(Key::Center);
  assert(fourText().find("LEAP denkt") != std::string::npos);
  four.close();
  // Minutes, not hours: hunger becomes visible; exit persists the latest decay.
  fakeNow += 24 * PetState::DecayIntervalMs;
  petGame.tick();
  render(petGame);
  assert(gfx.text.find("Hunger") != std::string::npos);
  petGame.close();
  assert(savedPet().food < 55);
  std::cout << "PASS: real Games actions, four animation frames, timed reset, Preferences "
               "restart/corruption, Snake exit and highscore\n";
}
