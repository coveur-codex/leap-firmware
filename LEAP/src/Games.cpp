#include "Games.h"
#include "Audio.h"
#include "Motion.h"
#include "PetAssets.h"
#include "Protocol.h"
#include <time.h>
namespace leap {
static const uint8_t maze[7] = {0b0000010, 0b0111010, 0b0001000, 0b1101110,
                                0b0000000, 0b0111110, 0b0000000};
void Games::begin() {
  gamePrefs.begin("leap-games", false);
  PetState saved;
  if (gamePrefs.getBytesLength("pet") == sizeof(saved) &&
      gamePrefs.getBytes("pet", &saved, sizeof(saved)) == sizeof(saved) && saved.valid())
    pet = saved;
  highscore = constrain(gamePrefs.getInt("snake-best", 0), 0, SnakeState::Capacity - 3);
  petLast = petSavedAt = millis();
}
void Games::savePet() {
  petDirty = gamePrefs.putBytes("pet", &pet, sizeof(pet)) != sizeof(pet);
  petSavedAt = millis();
}
void Games::avatarPackage(const String &id, int version, JsonVariantConst manifest) {
  if (avatarId == id && avatarVersion == version && petManifest.as<JsonVariantConst>() == manifest)
    return;
  avatarId = id;
  avatarVersion = version;
  petManifest.set(manifest);
  petPresentation(petManifest, petAssets);
}
String Games::petBlob(const String &path) const {
  for (JsonObjectConst file : petManifest["files"].as<JsonArrayConst>())
    if (file["path"] == path)
      return storage.blob(file["sha256"].as<String>());
  return "";
}
void Games::close() {
  if (opened && petDirty)
    savePet();
  opened = running = false;
  petAction = -1;
}
void Games::start(const String &id) {
  kind = id;
  opened = true;
  petSelection = 0;
  petAction = -1;
  if (isSnake())
    snake.start(esp_random());
  started = last = millis();
  score = 0;
  length = 1;
  step = show = 0;
  x = y = 0;
  won = false;
  running = true;
  showing = id == "simon_motion";
  for (auto &s : sequence)
    s = esp_random() % 4;
}
void Games::input(Key key) {
  if (!running)
    return;
  if (isPet()) {
    petSelection = constrain(petSelection + (key == Key::Down) - (key == Key::Up), 0, 3);
    if (key == Key::Center) {
      pet.care(petSelection);
      petAction = petSelection;
      actionAt = millis();
      savePet();
      audio.tone(660, 60);
    }
    return;
  }
  if (isSnake()) {
    snake.turn(int(key));
    return;
  }
  if (kind == "hot_potato") {
    if (key == Key::Center) {
      score++;
      audio.tone(400 + score * 20, 50);
    }
    return;
  }
  if (kind == "simon_motion") {
    if (showing || key == Key::Center)
      return;
    int value = int(key);
    audio.tone(350 + value * 180);
    if (value != sequence[step]) {
      running = false;
      return;
    }
    if (++step == length) {
      score++;
      if (length == 32) {
        won = true;
        running = false;
      } else {
        length++;
        step = show = 0;
        last = millis() + 400;
        showing = true;
      }
    }
    return;
  }
  if (kind == "tilt_maze") {
    int nx = x + (key == Key::Right) - (key == Key::Left),
        ny = y + (key == Key::Down) - (key == Key::Up);
    if (nx >= 0 && nx < 7 && ny >= 0 && ny < 7 && !(maze[ny] & (1 << nx))) {
      x = nx;
      y = ny;
    }
    if (x == 6 && y == 6) {
      won = true;
      running = false;
      audio.tone(880, 300);
    }
  }
}
void Games::tick() {
  uint32_t now = millis();
  if (uint32_t(now - petLast) >= PetState::DecayIntervalMs) {
    unsigned ticks = uint32_t(now - petLast) / PetState::DecayIntervalMs;
    petLast += ticks * PetState::DecayIntervalMs;
    PetState before = pet;
    pet.decay(ticks);
    if (before.food != pet.food || before.joy != pet.joy || before.clean != pet.clean ||
        before.energy != pet.energy)
      petDirty = true;
  }
  // Persist care immediately; batch passive decay to avoid a flash write per tick.
  if (petDirty && elapsed(now, petSavedAt, 300000))
    savePet();
  if (petAction >= 0 && elapsed(now, actionAt, 3000))
    petAction = -1;
  if (isSnake() && opened && running && elapsed(now, last, 220)) {
    last = now;
    if (snake.move(esp_random())) {
      audio.tone(880, 40);
      if (snake.score > highscore) {
        highscore = snake.score;
        gamePrefs.putInt("snake-best", highscore);
      }
    }
    running = snake.alive;
    if (!running) {
      audio.tone(snake.won ? 880 : 220, 200);
    }
  }
  if (!opened || !running)
    return;
  if (kind == "tilt_maze" || (kind == "simon_motion" && !showing)) {
    int direction = motion.direction(kind == "simon_motion");
    if (direction >= 0)
      input(Key(direction));
  }
  if (kind == "hot_potato" && motion.shake())
    input(Key::Center);
  if (kind == "hot_potato" && elapsed(millis(), started, 15000)) {
    running = false;
    won = score > 0;
    audio.tone(220, 300);
  }
  if (kind == "simon_motion" && showing && int32_t(millis() - last) >= 650) {
    last = millis();
    if (show < length) {
      audio.tone(350 + sequence[show] * 180, 180);
      show++;
    } else {
      showing = false;
      step = 0;
    }
  }
}
void Games::draw(Arduino_GFX &gfx, int left, int top) {
  if (isPet()) {
    drawPet(gfx);
    return;
  }
  if (isSnake()) {
    gfx.setTextColor(0xffff);
    gfx.setTextSize(1);
    gfx.setCursor(left, top);
    gfx.printf("Snake   Punkte %d   Rekord %d", snake.score, highscore);
    int boardY = top + 17;
    gfx.fillRect(left - 1, boardY - 1, 322, 106, 0x18c3);
    gfx.drawRect(left - 1, boardY - 1, 322, 106, 0x9d35);
    for (int i = 0; i < snake.length; ++i)
      gfx.fillRect(left + snake.body[i].x * 8, boardY + snake.body[i].y * 8, 7, 7,
                   i == 0 ? 0x07ff : 0x06b8);
    if (!snake.won)
      gfx.fillCircle(left + snake.food.x * 8 + 3, boardY + snake.food.y * 8 + 3, 3, 0xf800);
    if (!running) {
      gfx.fillRoundRect(left + 42, boardY + 30, 236, 43, 5, 0x10e5);
      gfx.setCursor(left + 55, boardY + 38);
      gfx.print(snake.won ? "Geschafft!" : "Spielende");
      gfx.setCursor(left + 55, boardY + 55);
      gfx.print("R Mitte: Neustart  L Mitte: Zurueck");
    }
    return;
  }
  gfx.setTextColor(0xffff);
  gfx.setTextSize(2);
  gfx.setCursor(left, top);
  if (kind == "hot_potato") {
    gfx.print("Heisse Kartoffel");
    gfx.setCursor(left, top + 25);
    gfx.printf("%d Treffer", score);
    gfx.setTextSize(1);
    gfx.setCursor(left, top + 55);
    gfx.print(running ? "Rechts Mitte: weitergeben!" : "Fertig! Mitte: neue Runde");
  } else if (kind == "simon_motion") {
    gfx.print("Simon");
    gfx.setTextSize(1);
    gfx.setCursor(left, top + 25);
    gfx.printf("Runde %d: %s", length, showing ? "Merken" : "Nachspielen");
    const char *labels[] = {"OBEN", "UNTEN", "LINKS", "RECHTS"};
    if (showing && show > 0 && !elapsed(millis(), last, 450)) {
      gfx.setTextSize(2);
      gfx.setCursor(left, top + 45);
      gfx.print(labels[sequence[show - 1]]);
    }
    if (!running) {
      gfx.setCursor(left, top + 70);
      gfx.print(won ? "Geschafft!" : "Noch einmal?");
    }
  } else {
    for (int row = 0; row < 7; row++)
      for (int col = 0; col < 7; col++) {
        int xx = left + col * 12, yy = top + row * 12;
        gfx.fillRect(xx, yy, 11, 11, maze[row] & (1 << col) ? 0x4a69 : 0x18c3);
      }
    gfx.fillCircle(left + x * 12 + 5, top + y * 12 + 5, 4, 0x07ff);
    gfx.drawRect(left + 72, top + 72, 11, 11, 0xffe0);
    gfx.setTextSize(1);
    gfx.setCursor(left + 100, top + 15);
    gfx.print(won ? "Ziel erreicht!" : "Finde den Weg");
    gfx.setCursor(left + 100, top + 35);
    gfx.print(motion.available ? "Kippen / Schalter" : "Rechter Schalter");
  }
}
void Games::drawPet(Arduino_GFX &gfx) {
  time_t now = time(nullptr);
  struct tm local {};
  localtime_r(&now, &local);
  bool night = petAction == 3 || petNight(now > 1700000000, local.tm_hour);
  gfx.fillRect(86, 0, 256, 142, night ? 0x1086 : 0xb6ff);
  JsonVariantConst definition = petAssets;
  String background = definition["backgrounds"][night ? "night" : "day"] | "";
  // A complete local scene also covers missing packages and asynchronous decoding.
  bool backgroundShown =
      background.length() && petBackground.draw(gfx, petBlob(background), background, 86, 0, 256,
                                                142, false, night ? 0x1086 : 0xb6ff);
  if (!backgroundShown) {
    gfx.fillCircle(310, 36, 12, night ? 0xffde : 0xffe0);
    if (night) {
      gfx.fillCircle(315, 31, 11, 0x1086);
      for (int i = 0; i < 7; ++i)
        gfx.fillRect(104 + i * 29, 27 + (i % 3) * 11, 2, 2, 0xffff);
    } else {
      gfx.fillRoundRect(112, 32, 42, 10, 5, 0xffff);
      gfx.fillRoundRect(270, 56, 34, 8, 4, 0xffff);
    }
    gfx.fillRoundRect(86, 95, 256, 47, 18, night ? 0x1a68 : 0x6d69);
    gfx.fillRect(86, 113, 256, 29, night ? 0x1245 : 0x4545);
  }
  const char *actionNames[] = {"eating", "playing", "happy", "sleeping"};
  const char *mood = petAction >= 0 ? actionNames[petAction] : pet.mood();
  auto animation = definition["animations"][mood];
  auto frames = animation["frames"].as<JsonArrayConst>();
  if (!frames.size()) {
    animation = definition["animations"]["idle"];
    frames = animation["frames"].as<JsonArrayConst>();
  }
  String path;
  if (frames.size()) {
    uint32_t ms = std::max(250, animation["frameDurationMs"] | 400);
    uint32_t clock = petAction >= 0 ? millis() - actionAt : millis();
    path = frames[(clock / ms) % frames.size()].as<String>();
  }
  if (!path.length())
    path = avatarFrame(petManifest, millis()).c_str();
  // Small whole-body motion keeps even subtle four-frame assets lively.
  bool resting = String(mood) == "sleeping" || String(mood) == "tired";
  int beat = (millis() / 200) % 8;
  int lift = resting ? 0 : (beat < 4 ? beat : 7 - beat);
  if (String(mood) == "playing" || String(mood) == "happy")
    lift *= 2;
  int petX = 174 + (String(mood) == "playing" ? (beat - 3) * 2 : 0);
  int petY = 34 - lift;
  bool shown =
      suffix(path.c_str(), ".png") &&
      petImage.draw(gfx, petBlob(path), path, petX, petY, 80, 80, false, 0x10e5, true, true, true);
  if (!shown) {
    int cx = petX + 40, cy = petY + 40;
    gfx.fillCircle(cx, cy, 30, 0x06b8);
    if (resting) {
      gfx.drawLine(cx - 14, cy - 6, cx - 6, cy - 6, 0x0000);
      gfx.drawLine(cx + 6, cy - 6, cx + 14, cy - 6, 0x0000);
    } else {
      gfx.fillCircle(cx - 10, cy - 6, 3, 0x0000);
      gfx.fillCircle(cx + 10, cy - 6, 3, 0x0000);
    }
    if (String(mood) == "hungry" || String(mood) == "eating")
      gfx.fillCircle(cx, cy + 12, 4, 0x0000);
    else {
      int corner = String(mood) == "sad" ? 17 : 10;
      gfx.drawLine(cx - 7, cy + corner, cx, cy + 14, 0x0000);
      gfx.drawLine(cx, cy + 14, cx + 7, cy + corner, 0x0000);
    }
    if (String(mood) == "dirty") {
      gfx.fillCircle(cx - 18, cy + 9, 4, 0x8300);
      gfx.fillCircle(cx + 14, cy + 18, 3, 0x8300);
    }
  }
  const char *status = "Mir geht es gut";
  if (String(mood) == "happy")
    status = "Ich bin gluecklich!";
  if (String(mood) == "hungry")
    status = "Ich habe Hunger!";
  if (String(mood) == "tired")
    status = "Ich bin muede";
  if (String(mood) == "dirty")
    status = "Bitte wasch mich";
  if (String(mood) == "sad")
    status = "Spiel mit mir!";
  if (petAction == 0)
    status = "Mjam, danke!";
  if (petAction == 1)
    status = "Juhu, spielen!";
  if (petAction == 2)
    status = "Wieder sauber!";
  if (petAction == 3)
    status = "Zzz... gute Nacht";
  gfx.fillRoundRect(94, 4, 240, 17, 5, 0x18e7);
  gfx.setTextSize(1);
  gfx.setTextColor(0xffff);
  gfx.setCursor(101, 9);
  gfx.print(status);
  if (resting) {
    gfx.setCursor(258, 38 + beat / 2);
    gfx.print("z Z");
  } else if (String(mood) == "happy" || String(mood) == "playing") {
    gfx.fillCircle(264, 53 - lift, 3, 0xfbc0);
    gfx.drawLine(258, 53 - lift, 270, 53 - lift, 0xfbc0);
    gfx.drawLine(264, 47 - lift, 264, 59 - lift, 0xfbc0);
  }
  const char *needs[] = {"Satt", "Spass", "Sauber", "Kraft"};
  const uint8_t values[] = {pet.food, pet.joy, pet.clean, pet.energy};
  gfx.fillRect(86, 117, 256, 25, 0x18e7);
  for (int i = 0; i < 4; ++i) {
    int x = 90 + i * 63;
    uint16_t color = values[i] < 55 ? 0xf9a0 : values[i] < 80 ? 0xffe0 : 0x07e0;
    gfx.setTextColor(0xffff);
    gfx.setCursor(x, 120);
    gfx.print(needs[i]);
    gfx.fillRect(x, 132, 57, 5, 0x4a69);
    gfx.fillRect(x, 132, 57 * values[i] / 100, 5, color);
  }
  gfx.fillRect(342, 0, 86, 142, 0x18e7);
  const char *labels[] = {"Fuettern", "Spielen", "Waschen", "Schlafen"};
  gfx.setTextSize(1);
  for (int i = 0; i < 4; ++i) {
    int y = 13 + i * 26;
    if (petSelection == i)
      gfx.fillRoundRect(346, y - 5, 78, 22, 4, 0x06b8);
    gfx.setTextColor(petSelection == i ? 0x10e5 : 0xffff);
    gfx.setCursor(350, y);
    gfx.print(labels[i]);
  }
  gfx.setTextColor(0x9d35);
  gfx.setCursor(348, 121);
  gfx.print("R Mitte: OK");
  gfx.setCursor(348, 133);
  gfx.print("L: Zurueck");
}
} // namespace leap
