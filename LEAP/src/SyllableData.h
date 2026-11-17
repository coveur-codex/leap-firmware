#pragma once
#include "DisplayText.h"
#include <string>
#include <vector>
namespace leap {
struct ReadingEntry {
  std::string word;
  std::vector<std::string> syllables;
  int level = 1;
};
inline bool readingLetters(const std::string &text, size_t maximum) {
  if (text.empty() || text.size() > maximum * 2) return false;
  size_t count = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = text[i];
    if (c >= 'A' && c <= 'Z') { ++count; continue; }
    if (c != 0xc3 || ++i >= text.size()) return false;
    c = text[i];
    if (c != 0x84 && c != 0x96 && c != 0x9c) return false;
    ++count;
  }
  return count <= maximum;
}
inline bool validReadingEntry(const ReadingEntry &entry, bool single = false) {
  if (entry.level < 1 || entry.level > 3 || !readingLetters(entry.word, single ? 6 : 16) ||
      entry.syllables.size() < (single ? 1u : 2u) || entry.syllables.size() > (single ? 1u : 5u)) return false;
  std::string joined;
  for (const auto &part : entry.syllables) {
    if (!readingLetters(part, 6)) return false;
    joined += part;
  }
  return joined == entry.word;
}
// Editorial syllabification, never inferred on the device. Add entries here or via config.
inline std::vector<ReadingEntry> readingWords() {
  return {{"MAMA", {"MA", "MA"}, 1}, {"PAPA", {"PA", "PA"}, 1},
          {"LAMA", {"LA", "MA"}, 1}, {"ROSE", {"RO", "SE"}, 1},
          {"SOFA", {"SO", "FA"}, 1}, {"HASE", {"HA", "SE"}, 1},
          {"NASE", {"NA", "SE"}, 1}, {"LIMO", {"LI", "MO"}, 1},
          {"OMA", {"O", "MA"}, 1}, {"OPA", {"O", "PA"}, 1},
          {"DOSE", {"DO", "SE"}, 1}, {"HOSE", {"HO", "SE"}, 1},
          {"LUPE", {"LU", "PE"}, 1}, {"RABE", {"RA", "BE"}, 1},
          {"BANANE", {"BA", "NA", "NE"}, 2}, {"TOMATE", {"TO", "MA", "TE"}, 2},
          {"LIMONADE", {"LI", "MO", "NA", "DE"}, 2}, {"MELONE", {"ME", "LO", "NE"}, 2},
          {"RAKETE", {"RA", "KE", "TE"}, 2}, {"SALAMI", {"SA", "LA", "MI"}, 2},
          {"ANANAS", {"A", "NA", "NAS"}, 2}, {"PIRAT", {"PI", "RAT"}, 2},
          {"BLUME", {"BLU", "ME"}, 3}, {"SCHULE", {"SCHU", "LE"}, 3},
          {"FENSTER", {"FENS", "TER"}, 3}, {"SCHOKOLADE", {"SCHO", "KO", "LA", "DE"}, 3},
          {"SCHMETTERLING", {"SCHMET", "TER", "LING"}, 3},
          {"LESEN", {"LE", "SEN"}, 2}, {"BÜCHER", {"BÜ", "CHER"}, 3}};
}
inline std::vector<ReadingEntry> readingSyllables() {
  std::vector<ReadingEntry> result;
  for (const char *s : {"MA", "ME", "MI", "MO", "MU", "LA", "LE", "LI", "LO", "LU",
                        "SA", "SE", "SI", "SO", "SU", "RA", "RE", "RI", "RO", "RU",
                        "TA", "TE", "TI", "TO", "TU", "NA", "NE", "NI", "NO", "NU",
                        "HA", "HE", "HI", "HO", "HU", "PA", "PE", "PI", "PO", "PU",
                        "BA", "BE", "BI", "BO", "BU", "FA", "FE", "FI", "FO", "FU"})
    result.push_back({s, {s}, 1});
  for (const char *s : {"AM", "IM", "OM", "AN", "IN", "UN", "MAL", "ROT", "LOS", "NAS", "RAT", "SEN"})
    result.push_back({s, {s}, 2});
  for (const char *s : {"BLA", "BLU", "BRA", "TRA", "SCHU", "SCHO", "FENS", "SCHMET", "LING", "CHER"})
    result.push_back({s, {s}, 3});
  return result;
}
} // namespace leap
