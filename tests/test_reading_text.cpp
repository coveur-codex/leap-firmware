#include "ReadingText.h"
#include "SyllableData.h"
#include <cassert>
#include <iostream>
#include <iterator>

using namespace leap;
struct Display {
  const GFXfont *font = nullptr;
  int size = 6, x = 0, y = 0;
  uint16_t color = 0;
  std::string printed;
  void setFont(const GFXfont *value = nullptr) { font = value; }
  void setTextSize(int value) { size = value; }
  void setTextColor(uint16_t value) { color = value; }
  void setCursor(int xx, int yy) { x = xx; y = yy; }
  void print(const char *value) { assert(font && size == 1); printed = value; }
};

int main() {
  const size_t sizes[] = {sizeof(ReadingBitmap14), sizeof(ReadingBitmap21), sizeof(ReadingBitmap28),
                          sizeof(ReadingBitmap35), sizeof(ReadingBitmap42)};
  assert(std::size(ReadingFonts) == 5 && std::size(ReadingMetrics) == 5);
  for (int face = 0; face < 5; ++face) {
    const auto &font = ReadingFonts[face];
    const auto &metrics = ReadingMetrics[face];
    assert(font.glyph['H' - font.first].height == metrics.capHeight);
    assert(metrics.height <= 64);
    if (face) assert(metrics.capHeight > ReadingMetrics[face - 1].capHeight);
    for (unsigned char c : displayGlyphs("ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ-")) {
      const auto &g = font.glyph[c - font.first];
      assert(g.width && g.height && g.xAdvance);
      assert(size_t(g.bitmapOffset + (g.width * g.height + 7) / 8) <= sizes[face]);
      assert(g.yOffset + metrics.ascent >= 0);
      assert(g.yOffset + metrics.ascent + g.height <= metrics.height);
    }
    for (unsigned char c : displayGlyphs("ÄÖÜ")) {
      const auto &g = font.glyph[c - font.first];
      assert(-g.yOffset > metrics.capHeight); // Real umlaut dots above the capital.
    }
  }
  assert(ReadingMetrics[4].capHeight == 42); // Original large 7-pixel letters at 6x.
  assert(readingFace("MA") == 4 && readingFace("MA-MA") == 4 && readingFace("BANANE") == 4);
  assert(readingAdvance("WWW", ReadingFonts[4]) > readingAdvance("III", ReadingFonts[4]));
  // Exact fit and a one-pixel deficit choose different pre-rendered faces.
  int exact = readingBounds("WWW", ReadingFonts[4]).width;
  assert(readingFace("WWW", exact) == 4 && readingFace("WWW", exact - 1) < 4);
  auto entries = readingSyllables();
  const auto words = readingWords(); entries.insert(entries.end(), words.begin(), words.end());
  entries.push_back({"ÄÖÜÄÖÜÄÖÜÄÖÜÄÖÜA", {"ÄÖÜÄ", "ÖÜÄÖ", "ÜÄÖÜ", "ÄÖÜA"}, 3});
  entries.push_back({"WWWWWWWWWWWWWWWW", {"WWWW", "WWWW", "WWW", "WWW", "WW"}, 3});
  for (const auto &entry : entries) for (bool split : {false, true}) {
    std::string value;
    for (const auto &part : entry.syllables) {
      if (split && !value.empty()) value += '-';
      value += displayGlyphs(part.c_str());
    }
    int face = readingFace(value);
    auto bounds = readingBounds(value, ReadingFonts[face]);
    assert(bounds.width <= 326);
    int cursor = 94 + (326 - bounds.width) / 2 - bounds.left;
    int baseline = 34 + (64 - ReadingMetrics[face].height) / 2 + ReadingMetrics[face].ascent;
    for (unsigned char c : value) {
      const auto &g = ReadingFonts[face].glyph[c - ReadingFonts[face].first];
      assert(cursor + g.xOffset >= 94 && cursor + g.xOffset + g.width <= 420);
      assert(baseline + g.yOffset >= 34 && baseline + g.yOffset + g.height <= 98);
      cursor += g.xAdvance;
    }
    Display gfx;
    printReadingPart(gfx, value, face, 100, baseline, 0x1234);
    assert(gfx.printed == value && gfx.color == 0x1234 && gfx.font == nullptr && gfx.size == 1);
  }
  std::cout << "PASS: native Noto Sans glyphs, 42px capitals, umlauts, proportional fit, long words, baseline and font restoration\n";
}
