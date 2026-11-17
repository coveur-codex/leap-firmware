#include "SyllableData.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <set>

using namespace leap;
bool vowel(char c) { return std::string("AEIOU").find(c) != std::string::npos; }
bool openSyllable(const std::string &s) {
  return (s.size() == 1 && vowel(s[0])) ||
         (s.size() == 2 && !vowel(s[0]) && vowel(s[1]));
}
bool simpleSyllable(const std::string &s) {
  return openSyllable(s) || (s.size() == 2 && vowel(s[0]) && !vowel(s[1])) ||
         (s.size() == 3 && !vowel(s[0]) && vowel(s[1]) && !vowel(s[2]));
}
int main() {
  const auto words = readingWords(), syllables = readingSyllables();
  std::array<size_t, 3> wordCount{}, syllableCount{};
  for (bool single : {false, true}) {
    std::set<std::string> unique;
    for (const auto &entry : single ? syllables : words) {
      assert(validReadingEntry(entry, single));
      assert(unique.insert(entry.word).second);
      (single ? syllableCount : wordCount)[entry.level - 1]++;
      if (entry.level == 1) {
        if (!single) assert(entry.syllables.size() == 2);
        for (const auto &part : entry.syllables) assert(openSyllable(part));
      }
      if (entry.level == 2) {
        if (!single) assert(entry.syllables.size() <= 4);
        for (const auto &part : entry.syllables) assert(simpleSyllable(part));
      }
    }
  }
  assert(wordCount[0] >= 50 && wordCount[1] >= 100 && wordCount[2] >= 150);
  assert(syllableCount[0] >= 70 && syllableCount[1] >= 60 && syllableCount[2] >= 100);
  // The original, approved curriculum keeps every entry at its existing level.
  const char *baseline[] = {
    "MAMA PAPA LAMA ROSE SOFA HASE NASE LIMO OMA OPA DOSE HOSE LUPE RABE",
    "BANANE TOMATE LIMONADE MELONE RAKETE SALAMI ANANAS PIRAT LESEN",
    "BLUME SCHULE FENSTER SCHOKOLADE SCHMETTERLING BÜCHER"};
  for (int level = 1; level <= 3; ++level) {
    std::string list = baseline[level - 1];
    size_t start = 0;
    do {
      auto end = list.find(' ', start);
      std::string word = list.substr(start, end - start);
      auto entry = std::find_if(words.begin(), words.end(), [&](const ReadingEntry &e) { return e.word == word; });
      assert(entry != words.end() && entry->level == level);
      if (end == std::string::npos) break;
      start = end + 1;
    } while (start < list.size());
  }
  // Editorial regression cases: ck stays together, tz/doubled consonants split,
  // pf can straddle a syllable boundary, and compounds retain their word boundary.
  const ReadingEntry examples[] = {
    {"BRÜCKE", {"BRÜ", "CKE"}, 3}, {"SCHNECKE", {"SCHNE", "CKE"}, 3},
    {"SPATZEN", {"SPAT", "ZEN"}, 3}, {"KNÖPFE", {"KNÖP", "FE"}, 3},
    {"KARTOFFEL", {"KAR", "TOF", "FEL"}, 2}, {"SONNENBLUME", {"SON", "NEN", "BLU", "ME"}, 3},
    {"SCHULTASCHE", {"SCHUL", "TA", "SCHE"}, 3}, {"HUBSCHRAUBER", {"HUB", "SCHRAU", "BER"}, 3},
    {"EICHHÖRNCHEN", {"EICH", "HÖRN", "CHEN"}, 3}};
  for (const auto &expected : examples) {
    auto found = std::find_if(words.begin(), words.end(), [&](const ReadingEntry &e) { return e.word == expected.word; });
    assert(found != words.end() && found->syllables == expected.syllables && found->level == expected.level);
  }
  std::cout << "PASS: expanded reading catalogue, unique explicit syllables, approved levels and editorial boundaries\n";
  for (size_t level = 0; level < 3; ++level)
    std::cout << "Level " << level + 1 << ": " << syllableCount[level] << " syllables, " << wordCount[level] << " words\n";
}
