#pragma once
#include "HeadingFont.h"
#include <algorithm>
#include <string>

namespace leap {
constexpr int QuizQuestionTop = 12;
// Leave a clear pixel between the last heading row and the answer highlight.
constexpr int QuizAnswersTop = std::max(49, QuizQuestionTop + HeadingLineHeight + HeadingHeight + 4);

struct TextBounds {
  int left = 0, width = 0;
};

// Include both ink overhang and cursor advance; narrow letters must not be
// wrapped as if they occupied the same cell as W or a German capital umlaut.
inline TextBounds displayTextBounds(const std::string &s, size_t start, size_t end,
                                    bool heading) {
  int cursor = 0, left = 0, right = 0;
  for (size_t i = start; i < end; ++i) {
    if (heading) {
      unsigned char c = s[i];
      if (c < HeadingFont.first || c > HeadingFont.last) c = '?';
      const auto &glyph = HeadingGlyphs[c - HeadingFont.first];
      if (glyph.width) {
        left = std::min(left, cursor + glyph.xOffset);
        right = std::max(right, cursor + glyph.xOffset + glyph.width);
      }
      cursor += glyph.xAdvance;
    } else {
      cursor += 6;
    }
    right = std::max(right, cursor);
  }
  return {left, right - left};
}

struct DisplayLine {
  size_t end, next;
  TextBounds bounds;
};

inline DisplayLine fitDisplayLine(const std::string &s, size_t start, int width,
                                  bool heading) {
  size_t end = start, space = std::string::npos;
  while (end < s.size() && s[end] != '\n') {
    if (displayTextBounds(s, start, end + 1, heading).width > width) break;
    if (s[end] == ' ') space = end;
    ++end;
  }
  if (end < s.size() && s[end] != '\n' && s[end] != ' ' && space > start &&
      space != std::string::npos)
    end = space;
  // The UI always supplies at least one glyph's width. Still make progress if
  // this helper is used with a smaller box later.
  if (end == start && end < s.size() && s[end] != '\n') ++end;
  size_t next = end;
  if (next < s.size() && (s[next] == ' ' || s[next] == '\n')) ++next;
  return {end, next, displayTextBounds(s, start, end, heading)};
}

template <typename Graphics>
void printHeadingLine(Graphics &gfx, const std::string &glyphs, int x, int y,
                      int left, uint16_t color) {
  gfx.setFont(&HeadingFont);
  gfx.setTextSize(1);
  gfx.setTextColor(color);
  gfx.setCursor(x - left, y + HeadingAscent);
  gfx.print(glyphs.c_str());
  gfx.setFont(); // Sidebar, answers and games continue using the built-in font.
}
} // namespace leap
