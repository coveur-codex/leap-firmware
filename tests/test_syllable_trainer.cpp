#include "SyllableTrainer.h"
#include "LearningSelection.h"
#include "GameSelection.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <random>
#include <set>
using namespace leap;
void bounds(const SyllableTrainer &trainer) {
  Arduino_GFX gfx;
  gfx.recordAll = gfx.recordTextBounds = true;
  trainer.draw(gfx);
  assert(gfx.font == nullptr); // The next hints/sidebar/page must keep its UI font.
  for (auto &run : gfx.fontRuns)
    if (run.font) assert(run.size == 1); // Native glyphs, never magnified 6x8 pixels.
  for (auto &r : gfx.rects)
    assert(r.x >= 86 && r.x + r.w <= 428 && r.y >= 0 && r.y + r.h <= 142);
}
void press(SyllableTrainer &t, Key k, uint32_t now = 0) { t.input({true, k, false}, now); }
int main(int argc, char **argv) {
  for (bool single : {false, true}) {
    auto entries = single ? readingSyllables() : readingWords();
    std::set<std::string> unique;
    std::set<int> levels;
    for (auto &entry : entries) {
      assert(validReadingEntry(entry, single));
      assert(unique.insert(entry.word).second);
      levels.insert(entry.level);
    }
    assert(levels.size() == 3);
  }
  assert(!validReadingEntry({"BANANE", {"BA", "NA"}, 1}));
  assert(!validReadingEntry({"BANANE", {"BANANE"}, 1}));
  assert(!validReadingEntry({"MAMA", {"MA", "MA"}, 4}));
  assert(!readingLetters("Mama", 6) && !readingLetters("M-A", 6));
  assert(readingLetters("ÄÖÜ", 3) && !readingLetters("ÄÖÜ", 2));
  assert(!readingLetters(std::string("MA") + char(0xc3), 6));
  JsonDocument config;
  assert(!deserializeJson(config, R"({"pages":[{"id":"games","enabled":true}],
    "games":[{"id":"math_lab","enabled":true},{"id":"snake","enabled":true}],
    "mathLab":{"targets":[9,10]}})"));
  assert(legacyMathLab(config));
  assert(enabledLearningApps(config) == std::vector<std::string>{"math_lab"});
  assert(enabledGames(config) == std::vector<std::string>{"snake"});
  auto page = config["pages"].as<JsonArray>().add<JsonObject>();
  page["id"] = "learning"; page["enabled"] = false;
  assert(!legacyMathLab(config) && enabledLearningApps(config).empty());
  page["enabled"] = true;
  auto apps = page["settings"]["applications"].to<JsonArray>();
  for (const char *id : {"math_lab", "syllable_trainer", "unknown", "math_lab"}) {
    auto app = apps.add<JsonObject>(); app["id"] = id; app["enabled"] = true;
  }
  assert(enabledLearningApps(config).size() == 2);
  apps[0]["enabled"] = apps[3]["enabled"] = false;
  assert(enabledLearningApps(config) == std::vector<std::string>{"syllable_trainer"});
  assert(config["mathLab"]["targets"][0] == 9);
  std::mt19937 random(819);
  SyllableTrainer t([&] { return random(); });
  t.configure({});
  // Holding the start button must not undo a help toggle that never happened.
  t.mode = 1; t.row = 0; press(t, Key::Center);
  t.input({true, Key::Center, true, 900}, 0);
  assert(t.separated && t.isMarked());
  t.loadMarks("");
  t.configure({});
  assert(t.mode == 0 && t.length == 10 && !t.autoSeconds && t.random && t.level == 1);
  for (int row = 0; row < 7; ++row) { t.row = row; bounds(t); }
  t.row = 0; press(t, Key::Center);
  auto first = t.current()->word;
  press(t, Key::Left); assert(t.position == 0);
  press(t, Key::Right); auto second = t.current()->word;
  assert(second != first && t.position == 1);
  press(t, Key::Left); assert(t.current()->word == first);
  auto tasks = t.tasks;
  t.input({false, Key::Right, false}, 0);
  assert(t.tasks == tasks && t.position == 0); // Left controls belong to Ui.
  press(t, Key::Down); assert(t.mode == 1 && t.separated);
  auto word = t.current()->word;
  press(t, Key::Center); assert(!t.separated && t.current()->word == word);
  press(t, Key::Center); assert(t.separated);
  press(t, Key::Center); // Hardware always emits a short press before a hold.
  t.input({true, Key::Center, true, 900}, 0);
  assert(t.isMarked() && t.separated && t.marksChanged);
  t.input({true, Key::Center, true, 2000}, 0); assert(t.isMarked());
  auto saved = t.saveMarks(); t.loadMarks(saved); assert(t.isMarked());
  press(t, Key::Down); assert(t.mode == 2 && !t.separated);
  press(t, Key::Center); assert(t.separated);
  press(t, Key::Up); assert(t.mode == 1 && t.separated);
  press(t, Key::Up); assert(t.mode == 0 && !t.separated);
  for (bool shuffle : {false, true}) for (int level : {1, 2, 3}) for (int mode : {0, 1, 2}) {
    t.configure({}); t.random = shuffle; t.level = level; t.mode = mode;
    for (int round = 0; round < 20; ++round) {
      std::string last = t.current() ? t.current()->word : "";
      assert(t.startRound(0));
      assert(t.tasks.size() == size_t(t.length));
      for (int i = 0; i < t.length; ++i) {
        assert(t.current()->level == level && t.current()->word != last);
        last = t.current()->word;
        bounds(t); press(t, Key::Center); bounds(t);
        press(t, Key::Right);
      }
      assert(t.screen == SyllableTrainer::Screen::Complete);
      bounds(t);
      press(t, Key::Left); assert(t.position == size_t(t.length - 1));
      press(t, Key::Right);
      press(t, Key::Center); assert(t.position == 0 && t.screen == SyllableTrainer::Screen::Exercise);
    }
  }
  JsonDocument settings;
  assert(!deserializeJson(settings, R"({"levels":[2,2,0,4],"defaultMode":2,"sessionLength":15,
    "random":false,"autoSeconds":5,
    "syllables":[{"text":"VE","level":1},{"text":"a"}],
    "words":[{"word":"WIESE","syllables":["WIE","SE"],"level":2},
    {"word":"FALSCH","syllables":["FA","SE"]},
    {"word":"LAMA","syllables":["LAM","A"],"level":3},
    {"word":"ÄÖÜÄÖÜÄÖÜÄÖÜÄÖÜA","syllables":["ÄÖÜÄ","ÖÜÄÖ","ÜÄÖÜ","ÄÖÜA"],"level":2}]})"));
  t.configure(settings);
  assert(t.levels == std::vector<int>{2} && t.level == 2 && t.length == 15 && t.mode == 2 && !t.random);
  assert(t.words.size() == readingWords().size() + 2);
  assert(t.syllables.size() == readingSyllables().size() + 1);
  for (auto &e : t.words) if (e.word == "LAMA") assert(e.level == 1 && e.syllables[0] == "LA");
  assert(t.startRound(UINT32_MAX - 1000));
  assert(!t.tick(3998, true));
  assert(t.tick(3999, true) && t.position == 1);
  assert(!t.tick(20000, false));
  assert(!t.tick(24999, true));
  assert(t.tick(25000, true));
  press(t, Key::Center, 29000);
  assert(!t.tick(33999, true));
  t.marked.clear();
  assert(!t.startRound(0, true));
  press(t, Key::Center); t.input({true, Key::Center, true, 900}, 0);
  word = t.current()->word;
  assert(t.startRound(0, true));
  for (auto index : t.tasks) assert(t.words[index].word == word); // One marked word may repeat.
  // Persisted keys distinguish syllables and words, independent of list order.
  t.loadMarks("S:MA\nW:BANANE\nW:BANANE\nW:<bad>\n");
  assert(t.marked.size() == 2);
  t.mode = 0; t.level = 1; assert(t.repeatCount() == 1);
  t.mode = 1; t.level = 2; assert(t.repeatCount() == 1);
  // Every custom word renders at both split states within the real 428x142 layout.
  for (size_t i = 0; i < t.words.size(); ++i) {
    t.tasks = {i}; t.position = 0; t.screen = SyllableTrainer::Screen::Exercise;
    for (bool separated : {false, true}) { t.separated = separated; bounds(t); }
  }
  t.configure({}); t.random = false;
  assert(t.startRound(0)); auto ordered = t.tasks;
  for (int i = 0; i < t.length; ++i) press(t, Key::Right);
  assert(t.startRound(0)); assert(t.tasks.front() == ordered.back() + 1);
  // The expanded catalogue must be reachable in fixed order, beyond the first round.
  for (int length : {10, 15}) for (int mode : {0, 1, 2}) for (int level : {1, 2, 3}) {
    t.configure({}); t.random = false; t.mode = mode; t.level = level; t.length = length;
    const auto &entries = mode == 0 ? t.syllables : t.words;
    size_t available = 0;
    for (const auto &entry : entries) available += entry.level == level;
    std::set<std::string> seen;
    for (size_t round = 0; round <= available / length; ++round) {
      assert(t.startRound(0));
      for (int i = 0; i < length; ++i) { seen.insert(t.current()->word); press(t, Key::Right); }
    }
    assert(seen.size() == available);
  }
  // Changing pool must not consume another mode's or level's fixed position.
  t.configure({}); t.random = false; t.mode = 0; t.level = 1;
  assert(t.startRound(0)); auto syllableRound = t.tasks;
  t.mode = 1; assert(t.startRound(0)); auto wordRound = t.tasks;
  t.mode = 0; t.level = 2; assert(t.startRound(0));
  t.level = 1; assert(t.startRound(0)); assert(t.tasks.front() == syllableRound.back() + 1);
  t.mode = 1; t.loadMarks("W:MAMA\n"); assert(t.startRound(0, true));
  for (auto index : t.tasks) assert(t.words[index].word == "MAMA");
  assert(t.startRound(0)); assert(t.tasks.front() == wordRound.back() + 1);
  // Mark keys stay stable when catalogue entries are inserted or reordered.
  t.loadMarks("S:MA\nW:BANANE\nW:BÜCHER\n");
  t.mode = 0; t.level = 1; assert(t.repeatCount() == 1);
  t.mode = 1; t.level = 2; assert(t.repeatCount() == 1);
  t.level = 3; assert(t.repeatCount() == 1);
  std::string offline; serializeJson(settings, offline);
  JsonDocument restored; assert(!deserializeJson(restored, offline));
  t.configure(restored); assert(t.level == 2 && t.mode == 2 && t.autoSeconds == 5);
  if (argc > 1) {
    JsonDocument fixture;
    std::ifstream stream(argv[1]);
    assert(!deserializeJson(fixture, stream));
    t.configure(learningSettings(fixture["config"])["syllableTrainer"]);
    assert(t.level == 2 && t.mode == 2 && t.length == 15);
    assert(t.words.back().word == "WIESE" && t.words.back().syllables[0] == "WIE");
    assert(t.startRound(0));
    bounds(t);
  }
  std::cout << "PASS: reading data, learning permissions/migration, modes, history, shuffle, repeats, timer rollover, config, offline and display bounds\n";
}
