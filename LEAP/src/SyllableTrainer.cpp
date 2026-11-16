#include "SyllableTrainer.h"
#include "AccentColor.h"
#include <algorithm>
namespace leap {
SyllableTrainer::SyllableTrainer(std::function<uint32_t()> randomWord) : rng(randomWord) {}
void SyllableTrainer::configure(JsonVariantConst config) {
  screen = Screen::Setup;
  centerHelpToggled = false;
  row = 0; tasks.clear(); position = 0; repeatOnly = false;
  syllables = readingSyllables(); words = readingWords();
  levels.clear();
  for (JsonVariantConst value : config["levels"].as<JsonArrayConst>())
    if (value.is<int>() && value.as<int>() >= 1 && value.as<int>() <= 3 &&
        std::find(levels.begin(), levels.end(), value.as<int>()) == levels.end()) levels.push_back(value.as<int>());
  if (levels.empty()) levels = {1, 2, 3};
  level = levels.front();
  mode = config["defaultMode"].is<int>() ? std::max(0, std::min(2, config["defaultMode"].as<int>())) : 0;
  length = config["sessionLength"] == 15 ? 15 : 10;
  random = config["random"] | true;
  int seconds = config["autoSeconds"] | 0;
  autoSeconds = seconds == 5 || seconds == 10 || seconds == 15 ? seconds : 0;
  auto append = [](std::vector<ReadingEntry> &out, JsonArrayConst rows, bool single) {
    size_t count = 0;
    for (JsonObjectConst value : rows) {
      if (count++ >= 32) break;
      ReadingEntry entry;
      entry.level = value["level"] | 1;
      entry.word = value[single ? "text" : "word"] | "";
      if (single) entry.syllables.push_back(entry.word);
      else for (JsonVariantConst part : value["syllables"].as<JsonArrayConst>()) {
        if (entry.syllables.size() >= 6) break;
        entry.syllables.push_back(part.as<std::string>());
      }
      if (validReadingEntry(entry, single) &&
          std::none_of(out.begin(), out.end(), [&](const ReadingEntry &e) { return e.word == entry.word; }))
        out.push_back(std::move(entry));
    }
  };
  append(syllables, config["syllables"].as<JsonArrayConst>(), true);
  append(words, config["words"].as<JsonArrayConst>(), false);
  resetHelp();
}
void SyllableTrainer::loadMarks(const std::string &saved) {
  marked.clear();
  size_t start = 0;
  while (start < saved.size() && marked.size() < 32) {
    size_t end = saved.find('\n', start);
    if (end == std::string::npos) break;
    std::string key = saved.substr(start, end - start);
    if (key.size() > 2 && key[1] == ':' && (key[0] == 'S' || key[0] == 'W') &&
        readingLetters(key.substr(2), key[0] == 'S' ? 6 : 16) &&
        std::find(marked.begin(), marked.end(), key) == marked.end()) marked.push_back(key);
    start = end + 1;
  }
  marksChanged = false;
}
std::string SyllableTrainer::saveMarks() const {
  std::string saved;
  for (const auto &key : marked) saved += key + '\n';
  return saved;
}
std::string SyllableTrainer::markKey(const ReadingEntry &entry) const {
  return std::string(mode == 0 ? "S:" : "W:") + entry.word;
}
const ReadingEntry *SyllableTrainer::current() const {
  if (tasks.empty() || position >= tasks.size() || tasks[position] >= entries().size()) return nullptr;
  return &entries()[tasks[position]];
}
bool SyllableTrainer::isMarked() const {
  auto entry = current();
  return entry && std::find(marked.begin(), marked.end(), markKey(*entry)) != marked.end();
}
size_t SyllableTrainer::repeatCount() const {
  size_t count = 0;
  for (const auto &entry : entries())
    if (entry.level == level && std::find(marked.begin(), marked.end(), markKey(entry)) != marked.end()) ++count;
  return count;
}
bool SyllableTrainer::startRound(uint32_t now, bool repeats) {
  std::vector<size_t> pool;
  const auto &all = entries();
  for (size_t i = 0; i < all.size(); ++i)
    if (all[i].level == level && (!repeats ||
        std::find(marked.begin(), marked.end(), markKey(all[i])) != marked.end())) pool.push_back(i);
  if (pool.empty()) return false;
  repeatOnly = repeats;
  tasks.clear();
  std::string previous = lastWord;
  while (tasks.size() < size_t(length)) {
    auto bag = pool;
    if (random) for (size_t i = bag.size(); i > 1; --i) std::swap(bag[i - 1], bag[rng() % i]);
    if (bag.size() > 1 && all[bag[0]].word == previous) std::swap(bag[0], bag[1]);
    for (size_t index : bag) {
      if (tasks.size() == size_t(length)) break;
      tasks.push_back(index);
      previous = all[index].word;
    }
  }
  screen = Screen::Exercise; position = 0; resetHelp(); changedAt = now;
  lastWord = current()->word;
  return true;
}
void SyllableTrainer::next(uint32_t now) {
  if (position + 1 >= tasks.size()) screen = Screen::Complete;
  else { ++position; lastWord = current()->word; resetHelp(); }
  changedAt = now;
}
void SyllableTrainer::input(const InputEvent &e, uint32_t now) {
  if (!e.right) return;
  changedAt = now;
  if (e.longPress) {
    if (screen == Screen::Exercise && e.key == Key::Center && e.heldMs < Input::ExtendedCenterHoldMs) {
      // Hardware sends the short press before the hold; restore its help toggle.
      if (centerHelpToggled) separated = !separated;
      centerHelpToggled = false;
      auto key = markKey(*current());
      auto found = std::find(marked.begin(), marked.end(), key);
      if (found != marked.end()) marked.erase(found);
      else if (marked.size() < 32) marked.push_back(key);
      marksChanged = true;
    }
    return;
  }
  centerHelpToggled = false;
  int direction = (e.key == Key::Down) - (e.key == Key::Up);
  int delta = (e.key == Key::Right) - (e.key == Key::Left);
  if (screen == Screen::Setup) {
    row = std::max(0, std::min(6, row + direction));
    if (row == 0 && e.key == Key::Center) startRound(now);
    if (row == 1 && (delta || e.key == Key::Center)) mode = (mode + 3 + (delta ? delta : 1)) % 3;
    if (row == 2 && (delta || e.key == Key::Center)) {
      auto index = std::find(levels.begin(), levels.end(), level) - levels.begin();
      level = levels[(index + levels.size() + (delta ? delta : 1)) % levels.size()];
    }
    if (row == 3 && (delta || e.key == Key::Center)) random = !random;
    if (row == 4 && (delta || e.key == Key::Center)) autoSeconds = (autoSeconds + 20 + (delta ? delta : 1) * 5) % 20;
    if (row == 5 && (delta || e.key == Key::Center)) length = length == 10 ? 15 : 10;
    if (row == 6 && e.key == Key::Center) startRound(now, true);
    resetHelp();
    return;
  }
  if (direction) {
    mode = (mode + 3 + direction) % 3;
    // An empty repeat pool in another mode returns to the setup, without unmarked tasks.
    if (!startRound(now, repeatOnly)) screen = Screen::Setup;
    return;
  }
  if (screen == Screen::Complete) {
    if (e.key == Key::Center || e.key == Key::Right) {
      if (!startRound(now, repeatOnly)) screen = Screen::Setup;
    }
    if (e.key == Key::Left) { screen = Screen::Exercise; resetHelp(); }
    return;
  }
  if (delta > 0) next(now);
  if (delta < 0 && position) { --position; lastWord = current()->word; resetHelp(); }
  if (e.key == Key::Center && mode != 0) { separated = !separated; centerHelpToggled = true; }
}
bool SyllableTrainer::tick(uint32_t now, bool visible) {
  if (!visible) { changedAt = now; return false; }
  if (screen != Screen::Exercise || !autoSeconds || !elapsed(now, changedAt, uint32_t(autoSeconds) * 1000)) return false;
  next(now);
  return true;
}
void SyllableTrainer::draw(Arduino_GFX &gfx, uint16_t accent) const {
  constexpr uint16_t White = 0xffff, Muted = 0x9d35, Yellow = 0xffe0;
  auto text = [&](const std::string &value, int x, int y, int scale, uint16_t color) {
    gfx.setTextSize(scale); gfx.setTextColor(color); gfx.setCursor(x, y);
    gfx.print(displayGlyphs(value.c_str()).c_str());
  };
  const char *modes[] = {"Silben lesen", "Silben verbinden", "Wörter lesen"};
  if (screen == Screen::Setup) {
    const char *names[] = {"", "Leicht", "Mittel", "Schwer"};
    std::vector<std::string> labels = {"Starten", std::string("Modus: ") + modes[mode],
      std::string("Stufe: ") + names[level], random ? "Reihenfolge: Zufall" : "Reihenfolge: Fest",
      autoSeconds ? "Wechsel: " + std::to_string(autoSeconds) + " Sekunden" : "Wechsel: Aus",
      "Runde: " + std::to_string(length) + " Aufgaben", "Wiederholen: " + std::to_string(repeatCount())};
    text("SilbenTrainer", 94, 5, 1, accent);
    for (int i = 0; i < 7; ++i) {
      if (i == row) gfx.drawRect(92, 19 + i * 15, 330, 14, accent);
      text(labels[i], 98, 22 + i * 15, 1, i == row ? White : Muted);
    }
    text("Mitte: Start / Wahl  Links Mitte: zurück", 94, 132, 1, Muted);
    return;
  }
  text(modes[mode], 94, 6, 1, Muted);
  text(std::to_string(position + 1) + "/" + std::to_string(tasks.size()), 382, 6, 1, Muted);
  if (screen == Screen::Complete) {
    text("Runde fertig!", 118, 45, 3, White);
    text("Mitte: Noch eine Runde", 124, 92, 2, accent);
    text("Links Mitte: zurück", 94, 128, 1, Muted);
    return;
  }
  const auto *entry = current();
  if (!entry) return;
  size_t letters = displayGlyphs(entry->word.c_str()).size() + (separated ? entry->syllables.size() - 1 : 0);
  int scale = std::min(6, 326 / int(letters * 6));
  int x = 94 + (326 - int(letters) * 6 * scale) / 2, y = 34 + (64 - 8 * scale) / 2;
  const uint16_t readingAccent = contrastInk(accent) == White ? White : accent;
  if (separated) {
    for (size_t i = 0; i < entry->syllables.size(); ++i) {
      if (i) { text("-", x, y, scale, White); x += 6 * scale; }
      text(entry->syllables[i], x, y, scale, i % 2 ? Yellow : readingAccent);
      x += displayGlyphs(entry->syllables[i].c_str()).size() * 6 * scale;
    }
  } else text(entry->word, x, y, scale, White);
  if (isMarked()) text("Gemerkt", 94, 104, 1, accent);
  text("< / > Aufgabe   Hoch/Runter: Modus", 94, 117, 1, Muted);
  text(mode ? "Mitte: Silbenhilfe  Halten: merken" : "Mitte halten: merken (max. 32)", 94, 132, 1, Muted);
}
} // namespace leap
