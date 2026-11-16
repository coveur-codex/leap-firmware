#pragma once
#include "SyllableData.h"
#include "Core.h"
#include "Input.h"
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <functional>
namespace leap {
class SyllableTrainer {
public:
  enum class Screen { Setup, Exercise, Complete };
  Screen screen = Screen::Setup;
  int mode = 0, level = 1, length = 10, autoSeconds = 0, row = 0;
  bool random = true, separated = false, repeatOnly = false;
  std::vector<int> levels{1, 2, 3};
  std::vector<ReadingEntry> syllables, words;
  std::vector<size_t> tasks;
  size_t position = 0;
  std::vector<std::string> marked;
  bool marksChanged = false;
  uint32_t changedAt = 0;
  explicit SyllableTrainer(std::function<uint32_t()> randomWord);
  void configure(JsonVariantConst config);
  void loadMarks(const std::string &saved);
  std::string saveMarks() const;
  const ReadingEntry *current() const;
  bool isMarked() const;
  size_t repeatCount() const;
  bool startRound(uint32_t now, bool repeats = false);
  void input(const InputEvent &event, uint32_t now);
  bool tick(uint32_t now, bool visible);
  void draw(Arduino_GFX &gfx, uint16_t accent = 0x06b8) const;

private:
  std::function<uint32_t()> rng;
  std::string lastWord;
  bool centerHelpToggled = false;
  const std::vector<ReadingEntry> &entries() const { return mode == 0 ? syllables : words; }
  std::string markKey(const ReadingEntry &entry) const;
  void resetHelp() { separated = mode == 1; }
  void next(uint32_t now);
};
} // namespace leap
