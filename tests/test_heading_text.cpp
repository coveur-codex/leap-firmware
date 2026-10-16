#include "DisplayText.h"
#include "HeadingText.h"
#include <cassert>
#include <iostream>

struct Display {
  const GFXfont *font = nullptr;
  int size = 2, x = 0, y = 0;
  uint16_t color = 0;
  std::string printed;
  void setFont(const GFXfont *f = nullptr) { font = f; }
  void setTextSize(int s) { size = s; }
  void setTextColor(uint16_t c) { color = c; }
  void setCursor(int xx, int yy) { x = xx; y = yy; }
  void print(const char *s) {
    assert(font == &leap::HeadingFont && size == 1);
    printed = s;
  }
};

int main() {
  using namespace leap;
  // All converted characters fit the advertised ascent/descent and bitmap array.
  for (const auto &g : HeadingGlyphs) {
    assert(g.yOffset + HeadingAscent >= 0);
    assert(g.yOffset + HeadingAscent + g.height <= HeadingHeight);
    assert(size_t(g.bitmapOffset + (g.width * g.height + 7) / 8) <= sizeof(HeadingBitmaps));
    assert(g.xAdvance > 0);
  }
  for (unsigned char c : displayGlyphs("äöüÄÖÜßéè°")) {
    const auto &g = HeadingGlyphs[c - HeadingFont.first];
    assert(g.width && g.height);
    assert(g.bitmapOffset != HeadingGlyphs['?' - HeadingFont.first].bitmapOffset);
  }
  assert(displayTextBounds("WWW", 0, 3, true).width >
         displayTextBounds("iii", 0, 3, true).width * 3);
  const auto &descender = HeadingGlyphs['j' - HeadingFont.first];
  assert(descender.yOffset + descender.height > 0);

  // Exact fits, word boundaries, explicit blank lines and a long unbroken token.
  auto exact = fitDisplayLine("WW rest", 0, displayTextBounds("WW", 0, 2, true).width, true);
  assert(exact.end == 2 && exact.next == 3);
  auto word = fitDisplayLine("Hallo Welt", 0, displayTextBounds("Hallo W", 0, 7, true).width, true);
  assert(word.end == 5 && word.next == 6);
  auto blank = fitDisplayLine("\nText", 0, 204, true);
  assert(blank.end == 0 && blank.next == 1);
  for (bool heading : {false, true}) {
    const auto source = displayGlyphs("Übergrößen WWWWWWiiiiii\n\nKüche und Grüße aus Köln!");
    size_t pos = 0;
    while (pos < source.size()) {
      auto row = fitDisplayLine(source, pos, 38, heading);
      assert(row.next > pos && row.next <= source.size());
      assert(row.bounds.width <= 38);
      for (size_t i = row.end; i < row.next; ++i)
        assert(source[i] == ' ' || source[i] == '\n');
      pos = row.next;
    }
  }
  // The aircraft column, image-bearing news column and quiz all fit typical text.
  for (auto example : {"DLH4AB", "Neue Grüße aus Köln", "Was gehört zur Küche?", "999 + 999 = ?"}) {
    auto glyphs = displayGlyphs(example);
    assert(fitDisplayLine(glyphs, 0, 196, true).end == glyphs.size());
  }
  // Two enlarged question rows cannot touch the selected answer's background.
  assert(QuizQuestionTop + HeadingLineHeight + HeadingHeight < QuizAnswersTop - 3);
  assert(QuizAnswersTop + 3*19 + 8 <= 132); // All four answers above the footer.
  assert(HeadingGlyphs['H' - HeadingFont.first].height == 11);
  assert(HeadingHeight == 17);
  Display display;
  printHeadingLine(display, "j", 94, 12, -1, 0x1234);
  assert(display.printed == "j" && display.x == 95 && display.y == 12 + HeadingAscent);
  assert(display.color == 0x1234 && display.font == nullptr);
  std::cout << "PASS: Helvetica Bold 10 glyphs, German characters, proportional wrapping, quiz bounds and font restoration\n";
}
