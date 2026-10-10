#include "DisplayText.h"
#include <cassert>
#include <iostream>
#ifdef LEAP_GFX_FONT_TEST
#include <font/glcdfont.h>
#endif

struct Display {
  std::string bytes;
  void print(const char *s) { bytes += s; }
};

int main() {
  using leap::displayGlyphs;
  assert(displayGlyphs("LEAP 123\nHallo!") == "LEAP 123\nHallo!");
  assert(displayGlyphs("äöüÄÖÜß") == std::string("\x84\x94\x81\x8e\x99\x9a\xe1", 7));
#ifdef LEAP_GFX_FONT_TEST
  // Verify the actual pinned library's pixels, also used by enlarged UI headings.
  const unsigned char germanBitmaps[][5] = {
      {0x22, 0x54, 0x54, 0x78, 0x42}, {0x3a, 0x44, 0x44, 0x44, 0x3a},
      {0x3a, 0x40, 0x40, 0x20, 0x7a}, {0x7d, 0x12, 0x11, 0x12, 0x7d},
      {0x3d, 0x42, 0x42, 0x42, 0x3d}, {0x3d, 0x40, 0x40, 0x40, 0x3d},
      {0xfc, 0x4a, 0x4a, 0x4a, 0x34}};
  auto german = displayGlyphs("äöüÄÖÜß");
  for (unsigned i = 0; i < german.size(); ++i)
    for (unsigned column = 0; column < 5; ++column)
      assert(font[static_cast<unsigned char>(german[i]) * 5 + column] == germanBitmaps[i][column]);
#endif
  assert(displayGlyphs("Menü: Grüße aus Köln!") == "Men\x81: Gr\x81\xe1" "e aus K\x94ln!");
  assert(displayGlyphs("„Café“ – 23°") == "\"Caf\x82\" - 23\xf8");
  assert(displayGlyphs("è — ’") == "\x8a - '");
  // One unsupported Unicode character becomes one cell, including four-byte emoji.
  assert(displayGlyphs("東京 🙂!") == "?? ?!");
  assert(displayGlyphs("\t\r\x01Hallo") == "Hallo");
  for (const char *bad : {"\xc3", "\xe2\x82", "\xf0\x9f\x99", "\xed\xa0\x80", "\xf4\x90\x80\x80"})
    assert(displayGlyphs(bad) == "?");
  assert(displayGlyphs("\xc3!") == "?!");
  assert(displayGlyphs("\xc0\xaf") == "??");

  // Clipping and fixed-width wrapping must operate on glyph bytes, not UTF-8 bytes.
  auto title = displayGlyphs("Übergrößen");
  assert(title.size() == 10);
  assert(title.substr(0, 4) == "\x9a" "ber");
  assert(title.substr(4, 3) == "gr\x94");
  assert(title.substr(7) == "\xe1" "en");
  const std::string source = "Küche leeren?";
  Display display;
  leap::printDisplayText(display, source.c_str());
  assert(display.bytes == "K\x81" "che leeren?");
  assert(source == "Küche leeren?"); // Stored/server text stays UTF-8.
  std::cout << "PASS: German glyphs, single-cell clipping, punctuation, invalid UTF-8 and display boundary\n";
}
