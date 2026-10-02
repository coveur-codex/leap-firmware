#include "Games.h"
#include "Audio.h"
#include "Motion.h"
namespace leap {
static const uint8_t maze[7] = {0b0000010, 0b0111010, 0b0001000, 0b1101110,
                                0b0000000, 0b0111110, 0b0000000};
void Games::start(const String &id) {
  kind = id;
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
  if (!running)
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
} // namespace leap
