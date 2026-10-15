#include "MathLab.h"
#include "DisplayText.h"
namespace leap {
namespace {
constexpr uint16_t Ink = 0xffff, Teal = 0x07ba, Gold = 0xffa0, Muted = 0x9492, Bg = 0x10e5;
const char *activities[] = {"Würfellabor", "Zahlenwerkstatt", "Würfelarchitekt", "Würfeldetektiv"};
const char *variants[4][4] = {
    {"Würfeln und zählen", "Zahlenkombinationen", "Würfe erforschen", ""},
    {"Mengen erkennen", "Zahlen zerlegen", "Mengen vergleichen", "Zehner und Einer"},
    {"Nachbauen", "Merken und nachbauen", "Genau hinschauen", ""},
    {"Fehlende Kante", "Fehler finden", "Richtige Ansicht", ""}};
int variantCount(int activity) {
  return activity == 1 ? 4 : 3;
}
void label(Arduino_GFX &g, int x, int y, const char *s, uint16_t c = Ink, int size = 1) {
  g.setTextColor(c);
  g.setTextSize(size);
  g.setCursor(x, y);
  printDisplayText(g, s);
}
void number(Arduino_GFX &g, int x, int y, int n, uint16_t c = Ink, int size = 2) {
  char s[16];
  snprintf(s, sizeof(s), "%d", n);
  label(g, x, y, s, c, size);
}
void die(Arduino_GFX &g, int x, int y, int n, uint16_t color) {
  g.fillRoundRect(x, y, 38, 38, 5, Ink);
  const int positions[6][6] = {{4, -1, -1, -1, -1, -1}, {0, 8, -1, -1, -1, -1},
                               {0, 4, 8, -1, -1, -1},   {0, 2, 6, 8, -1, -1},
                               {0, 2, 4, 6, 8, -1},     {0, 2, 3, 5, 6, 8}};
  for (int i = 0; i < n; ++i) {
    int p = positions[n - 1][i];
    g.fillCircle(x + 9 + (p % 3) * 10, y + 9 + (p / 3) * 10, 3, color);
  }
}
// Ordered five/ten frames; each outlined group contains exactly ten slots.
void quantity(Arduino_GFX &g, int x, int y, int n, int split = -1, int step = 10) {
  for (int block = 0; block < (n + 9) / 10; ++block) {
    int bx = x + (block % 5) * (5 * step + 4), by = y + (block / 5) * (2 * step + 5);
    g.drawRect(bx - 3, by - 3, 5 * step, 2 * step, Muted);
    for (int j = 0; j < 10; ++j) {
      int i = block * 10 + j, px = bx + (j % 5) * step, py = by + (j / 5) * step;
      g.fillCircle(px, py, step / 3, i < n ? (split >= 0 && i >= split ? Gold : Teal) : Bg);
    }
  }
}
void line(Arduino_GFX &g, mathlab::Point a, mathlab::Point b, int ox, int oy, uint16_t c) {
  g.drawLine(ox + a.x, oy + a.y, ox + b.x, oy + b.y, c);
}
void cube(Arduino_GFX &g, int ox, int oy, int scale, uint16_t c) {
  auto v = mathlab::vertices(scale);
  // Filled scanlines on three faces, avoiding additional graphics dependencies.
  for (int z = 0; z < scale; ++z) {
    g.drawLine(ox - scale, oy + scale / 2 - z, ox, oy + scale - z, c);
    g.drawLine(ox, oy + scale - z, ox + scale, oy + scale / 2 - z, c);
  }
  for (int t = 0; t < scale; ++t)
    g.drawLine(ox - t, oy - scale + t / 2, ox + scale - t, oy - scale / 2 + t / 2, c);
  for (auto &e : mathlab::edges)
    line(g, v[e[0]], v[e[1]], ox, oy, Ink);
}
void building(Arduino_GFX &g, const mathlab::Building &b, int ox, int oy, int cursor = -1,
              unsigned diff = 0) {
  const int s = 10;
  for (int depth = 0; depth <= 4; ++depth)
    for (int y = 0; y < b.size; ++y)
      for (int x = 0; x < b.size; ++x) {
        if (x + y != depth)
          continue;
        int i = y * 3 + x;
        auto p = mathlab::project(x, y, 0, s);
        uint16_t c = diff & (1u << i) ? Gold : Teal;
        auto v = mathlab::vertices(s);
        for (int e = 0; e < 4; ++e)
          line(g, v[mathlab::edges[e][0]], v[mathlab::edges[e][1]], ox + p.x, oy + p.y + s, c);
        for (int z = 0; z < b.heights[i]; ++z)
          cube(g, ox + p.x, oy + p.y - z * s, s, c);
        if (i == cursor) {
          g.drawRect(ox + p.x - 5, oy + p.y + 6, 11, 5, Gold);
        }
      }
}
} // namespace
void MathLab::start() {
  *this = MathLab{};
}
void MathLab::newTask() {
  feedback = correct = help = preview = false;
  answer = part = tens = ones = cursor = chosen = selectedPart = 0;
  a = mathlab::die(esp_random);
  b = mathlab::die(esp_random);
  if (activity == 0) {
    diceCount = difficulty == 0 ? 1 : 2;
    if (variant == 1) {
      target = 2 + mathlab::below(esp_random, 11);
      a = b = 1;
      found = 0;
    }
  } else if (activity == 1) {
    int limit = mathlab::numberLimit(difficulty);
    target = 1 + mathlab::below(esp_random, limit);
    a = mathlab::below(esp_random, limit + 1);
    b = mathlab::below(esp_random, limit + 1);
  } else {
    original = mathlab::generate(esp_random, difficulty, variant == 2 && activity == 2);
    built = {};
    built.size = original.size;
    if (activity == 2) {
      preview = true;
      previewAt = millis();
    } else if (variant < 2)
      solution = mathlab::below(esp_random, 3 + 3 * difficulty);
    else {
      solution = mathlab::below(esp_random, 3);
      views[solution] = original;
      for (int i = 0; i < 3; ++i)
        if (i != solution) {
          views[i] = original;
          // Distinct height fields, differing in one of two separate positions.
          ++views[i].heights[i == ((solution + 1) % 3) ? 0 : 1];
        }
    }
  }
}
void MathLab::check() {
  feedback = true;
  if (activity == 0) {
    correct = variant == 0 ? answer == a + (diceCount == 2 ? b : 0) : a + b == target;
    if (correct && variant == 1)
      found |= 1u << (std::min(a, b) - 1);
  } else if (activity == 1) {
    correct = variant == 0   ? answer == target
              : variant == 1 ? mathlab::decomposition(target, part, answer)
              : variant == 2 ? answer == (a < b    ? 0
                                          : a == b ? 1
                                                   : 2)
                             : 10 * tens + ones == target;
  } else if (activity == 2)
    correct = original == built;
  else
    correct = chosen == solution;
  help = true;
}
void MathLab::tick(uint32_t now) {
  if (screen == Screen::Play && activity == 2 && variant == 1 && preview &&
      uint32_t(now - previewAt) >= uint32_t(seconds) * 1000)
    preview = false;
}
bool MathLab::input(const InputEvent &e) {
  if (e.longPress)
    return false;
  int delta =
      (e.key == Key::Down || e.key == Key::Right) - (e.key == Key::Up || e.key == Key::Left);
  if (!e.right) {
    if (e.key == Key::Center) {
      if (screen == Screen::Activities)
        return true;
      screen = screen == Screen::Play ? Screen::Settings : Screen::Activities;
    } else if (screen == Screen::Play) {
      if (e.key == Key::Right)
        newTask();
      if (e.key == Key::Left) {
        help = !help;
        if (activity == 2 && variant != 1)
          preview = !preview;
      }
      if (e.key == Key::Up && activity == 2 && !preview)
        check();
      if (e.key == Key::Down) {
        if (activity == 2 && !preview) {
          if (built.heights[cursor])
            --built.heights[cursor];
          feedback = false;
        } else if (activity == 0 && variant == 0) {
          diceCount = 3 - diceCount;
          feedback = false;
        } else if (activity == 0 && variant == 1)
          selectedDie = 1 - selectedDie;
        else if (activity == 1 && (variant == 1 || variant == 3))
          selectedPart = 1 - selectedPart;
      }
    }
    return false;
  }
  if (screen == Screen::Activities) {
    activity = std::max(0, std::min(3, activity + delta));
    if (e.key == Key::Center) {
      variant = setting = 0;
      difficulty = std::min(difficulty, activity == 1 ? 3 : 2);
      screen = Screen::Settings;
    }
  } else if (screen == Screen::Settings) {
    if (e.key == Key::Up || e.key == Key::Down)
      setting = std::max(0, std::min(3, setting + delta));
    if (e.key == Key::Left || e.key == Key::Right) {
      if (setting == 0)
        variant = (variant + variantCount(activity) + delta) % variantCount(activity);
      if (setting == 1)
        difficulty = std::max(0, std::min(activity == 1 ? 3 : 2, difficulty + delta));
      if (setting == 2 && activity == 2)
        seconds = std::max(3, std::min(20, seconds + delta));
    }
    if (e.key == Key::Center) {
      screen = Screen::Play;
      histogram = {};
      newTask();
    }
  } else if (activity == 2) {
    if (preview) {
      if (e.key == Key::Center)
        preview = false;
    } else if (e.key == Key::Center) {
      built.heights[cursor] = (built.heights[cursor] + 1) % 4;
      feedback = false;
    } else {
      int x = cursor % 3, y = cursor / 3;
      if (e.key == Key::Left || e.key == Key::Right)
        x = std::max(0, std::min(built.size - 1, x + delta));
      else
        y = std::max(0, std::min(built.size - 1, y + delta));
      cursor = y * 3 + x;
    }
  } else if (activity == 0 && variant == 2) {
    if (e.key == Key::Center) {
      a = mathlab::die(esp_random);
      b = mathlab::die(esp_random);
      histogram.add(a, b);
    }
  } else if (activity == 3) {
    chosen = std::max(0, std::min(variant == 2 ? 2 : 2 + 3 * difficulty, chosen + delta));
    if (e.key == Key::Center)
      check();
    else
      feedback = help = false;
  } else {
    if (e.key == Key::Center) {
      check();
      return false;
    }
    feedback = false;
    if (activity == 0 && variant == 1) {
      int &v = selectedDie ? b : a;
      v = std::max(1, std::min(6, v + delta));
    } else if (activity == 1 && variant == 1) {
      int &v = selectedPart ? answer : part;
      v = std::max(0, std::min(target, v + delta));
    } else if (activity == 1 && variant == 3) {
      int &v = selectedPart ? ones : tens;
      v = std::max(0, std::min(selectedPart ? 9 : 10, v + delta));
    } else
      answer = std::max(0, std::min(activity == 0  ? 12
                                    : variant == 2 ? 2
                                                   : mathlab::numberLimit(difficulty),
                                    answer + delta));
  }
  return false;
}
void MathLab::draw(Arduino_GFX &g) {
  label(g, 94, 3, screen == Screen::Activities ? "Mathe-Labor" : activities[activity], Teal);
  if (screen == Screen::Activities) {
    label(g, 100, 20, "Zahlen entdecken", Muted);
    label(g, 100, 77, "Räumlich denken", Muted);
    for (int i = 0; i < 4; ++i) {
      int y = 34 + (i / 2) * 57 + (i % 2) * 20;
      if (i == activity)
        g.fillRoundRect(96, y - 3, 322, 17, 3, 0x2390);
      label(g, 104, y, activities[i]);
    }
    label(g, 94, 133, "R: wählen/OK   L OK: Spiele", Muted);
    return;
  }
  if (screen == Screen::Settings) {
    char s[52];
    for (int i = 0; i < 4; ++i) {
      int y = 27 + i * 24;
      if (setting == i)
        g.fillRoundRect(96, y - 4, 322, 20, 3, 0x2390);
      if (i == 0)
        label(g, 102, y, variants[activity][variant]);
      if (i == 1) {
        snprintf(s, sizeof(s), activity == 1 ? "Zahlenraum: bis %d" : "Baustufe / Stufe: %d",
                 activity == 1 ? mathlab::numberLimit(difficulty) : difficulty + 1);
        label(g, 102, y, s);
      }
      if (i == 2) {
        snprintf(s, sizeof(s), "Merkzeit: %d s", seconds);
        label(g, 102, y, activity == 2 ? s : "Alle Stufen frei wählbar");
      }
      if (i == 3)
        label(g, 102, y, "Los geht's!");
    }
    label(g, 94, 133, "R: Zeile / <> ändern / OK starten", Muted);
    return;
  }
  label(g, 94, 17, variants[activity][variant], Muted);
  if (activity == 0) {
    die(g, 110, 35, a, Teal);
    if (diceCount == 2 || variant != 0) {
      label(g, 158, 49, "+", Ink, 2);
      die(g, 180, 35, b, Gold);
    }
    if (variant == 2) {
      uint32_t max = 1;
      for (auto n : histogram.bins)
        max = std::max(max, n);
      for (int i = 0; i < 11; ++i) {
        int h = uint64_t(histogram.bins[i]) * 42 / max;
        g.fillRect(240 + i * 16, 95 - h, 10, h, Teal);
        number(g, 240 + i * 16, 99, i + 2, Ink, 1);
      }
      number(g, 110, 83, histogram.throws, Gold, 1);
      label(g, 145, 83, "Würfe");
      label(g, 94, 116, "Was ändert sich mit mehr Würfen?");
    } else if (variant == 1) {
      label(g, 245, 34, "Ziel");
      number(g, 289, 31, target, Gold);
      label(g, 245, 57, selectedDie ? "Würfel 2 ändern" : "Würfel 1 ändern");
      number(g, 245, 79, __builtin_popcount(found), Teal, 1);
      label(g, 263, 79, "Paare entdeckt");
    } else {
      label(g, 245, 35, "Wie viele?");
      number(g, 270, 53, answer, Gold);
    }
    if (help && variant != 2) {
      char s[40];
      snprintf(s, sizeof(s), "%d + %d = %d", a, (variant == 0 && diceCount == 1) ? 0 : b,
               a + ((variant == 0 && diceCount == 1) ? 0 : b));
      label(g, 110, 87, s, Teal, 2);
    }
  } else if (activity == 1) {
    if (variant == 0 || variant == 1) {
      quantity(g, 104, 39, target, variant == 1 ? part : -1, difficulty == 3 ? 6 : 10);
      if (variant == 0) {
        label(g, 104, 91, "Wie viele?");
        number(g, 185, 87, answer, Gold);
      } else {
        char s[40];
        snprintf(s, sizeof(s), "%d = %d + %d", target, part, answer);
        label(g, 104, 91, s, Gold, 2);
        label(g, 280, 94, selectedPart ? "Teil 2" : "Teil 1");
      }
      if (help && variant == 0)
        number(g, 340, 91, target, Teal);
    } else if (variant == 2) {
      quantity(g, 102, 38, a, -1, 5);
      quantity(g, 270, 38, b, -1, 5);
      const char *choices[] = {"Links kleiner <", "Gleich viel =", "Links größer >"};
      label(g, 154, 94, choices[answer], Gold);
      if (help) {
        number(g, 108, 87, a, Teal);
        number(g, 360, 87, b, Gold);
      }
    } else {
      label(g, 103, 33, "Baue");
      number(g, 140, 30, target, Gold);
      // Up to nine ten frames, each with ten visible points.
      quantity(g, 104, 59, tens * 10, -1, 6);
      quantity(g, 290, 59, ones, -1, 9);
      char s[48];
      snprintf(s, sizeof(s), "%d Z + %d E = %d", tens, ones, tens * 10 + ones);
      label(g, 104, 94, s, Teal);
      label(g, 270, 94, selectedPart ? "> Einer" : "> Zehner");
      if (help) {
        snprintf(s, sizeof(s), "%d Zehner + %d Einer", target / 10, target % 10);
        label(g, 104, 110, s, Gold);
      }
    }
  } else if (activity == 2) {
    unsigned diff = feedback ? original.differences(built) : 0;
    if (preview || variant == 0 || feedback) {
      building(g, original, 170, 70, -1, diff);
      label(g, 128, 100, "Vorlage");
    } else
      label(g, 106, 57, "Im Kopf ...", Muted);
    if (!preview || variant == 0 || feedback) {
      building(g, built, 337, 70, cursor, diff);
      label(g, 294, 100, "Dein Bau");
    } else
      label(g, 273, 55, "R OK: bauen", Gold);
    label(g, 94, 116,
          feedback ? (correct ? "Passt! Neues Bauwerk: L >" : "Gelb: Hier kannst du umbauen")
                   : "L ^ prüfen  v abbauen  < Vorlage  > neu",
          Gold);
  } else if (variant < 2) {
    auto v = mathlab::vertices(29);
    for (int i = 0; i < 9; ++i) {
      auto p = v[mathlab::edges[i][0]], q = v[mathlab::edges[i][1]];
      bool broken = i == solution;
      if (!broken || help)
        line(g, p, q, 169, 69, help && broken ? Gold : Ink);
      if (broken && variant == 1 && !help) {
        auto wrong = q;
        wrong.x += 15;
        line(g, p, wrong, 169, 69, Ink);
      }
      // Numbers sit at edge midpoints; the selected candidate is also drawn right.
      if (i < 3 + 3 * difficulty)
        number(g, 169 + (p.x + q.x) / 2, 69 + (p.y + q.y) / 2, i + 1, i == chosen ? Gold : Teal, 1);
    }
    label(g, 250, 35, "Welche Kante?");
    number(g, 290, 53, chosen + 1, Gold);
    label(g, 247, 81, help ? "Gelb zeigt die Lösung" : "R <> wählen / OK");
    if (help)
      label(g, 240, 99, "Kanten bleiben parallel", Gold);
  } else {
    building(g, original, 153, 64);
    label(g, 112, 98, "Vorlage");
    for (int i = 0; i < 3; ++i) {
      // Plan view exposes all columns, making hidden-stack ambiguities impossible.
      int ox = 235 + i * 60;
      for (int y = 0; y < original.size; ++y)
        for (int x = 0; x < original.size; ++x) {
          int n = views[i].heights[y * 3 + x];
          g.drawRect(ox + x * 15, 42 + y * 15, 15, 15, Muted);
          number(g, ox + 4 + x * 15, 46 + y * 15, n, help && i == solution ? Teal : Ink, 1);
        }
      number(g, ox + 14, 98, i + 1, chosen == i ? Gold : Ink, 1);
    }
    label(g, 225, 114, "Draufsicht: Zahl = Stapelhöhe", Muted);
  }
  if (feedback && activity != 2)
    label(g, 94, 121,
          correct ? "Entdeckt! Probiere weiter." : "Schau auf die farbigen Gruppen / Kanten.",
          correct ? Teal : Gold);
  const char *footer = "L OK zurück  < Hilfe  > neu | R wählen/OK";
  if (activity == 0 && variant == 0)
    footer = "L v: 1/2 Würfel  > Wurf  R wählen/OK";
  if (activity == 0 && variant == 1)
    footer = "L v: Würfel wechseln  R ändern/OK";
  if (activity == 0 && variant == 2)
    footer = "R OK: würfeln  L >: leeren  L OK zurück";
  if (activity == 1 && (variant == 1 || variant == 3))
    footer = "L v: Teil wechseln  R ändern/OK  > neu";
  if (activity == 2)
    footer = "R: Feld / OK +Würfel | L OK zurück";
  label(g, 94, 133, footer, Muted);
}
} // namespace leap
