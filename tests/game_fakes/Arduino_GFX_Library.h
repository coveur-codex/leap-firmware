#pragma once
#include <cstdarg>
#include <gfxfont.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
struct Arduino_GFX {
  const GFXfont *font = nullptr;
  struct FontRun { const GFXfont *font; int size; };
  std::vector<FontRun> fontRuns;
  void setFont(const GFXfont *next = nullptr) { font = next; }
  std::string text;
  std::vector<uint16_t> textColors;
  bool recordAll = false, recordTextBounds = false;
  int textX = 0, textY = 0, textSize = 1;
  struct Rect {
    int x, y, w, h;
    uint16_t color;
  };
  std::vector<Rect> rects;
  void fillScreen(uint16_t) {}
  void drawPixel(int x,int y,uint16_t c) { if (recordAll) rects.push_back({x,y,1,1,c}); }
  void drawFastHLine(int x,int y,int w,uint16_t c) { if (recordAll) rects.push_back({x,y,w,1,c}); }
  void setTextColor(uint16_t color) { textColors.push_back(color); }
  void setTextSize(int size) { textSize = size; }
  void setCursor(int x, int y) { textX = x; textY = y; }
  void print(const char *s) {
    text += s;
    if (recordTextBounds) {
      fontRuns.push_back({font, textSize});
      if (font) {
        int cursor = textX;
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p; ++p) {
          if (*p < font->first || *p > font->last) continue;
          const auto &glyph = font->glyph[*p - font->first];
          if (glyph.width && glyph.height)
            rects.push_back({cursor + glyph.xOffset * textSize, textY + glyph.yOffset * textSize,
                            glyph.width * textSize, glyph.height * textSize, 0});
          cursor += glyph.xAdvance * textSize;
        }
      } else rects.push_back({textX,textY,int(std::string(s).size())*6*textSize,8*textSize,0});
    }
  }
  void printf(const char *format, ...) {
    char out[256];
    va_list args;
    va_start(args, format);
    vsnprintf(out, sizeof(out), format, args);
    va_end(args);
    text += out;
  }
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    rects.push_back({x, y, w, h, color});
  }
  void drawRect(int x,int y,int w,int h,uint16_t c) { if (recordAll) rects.push_back({x,y,w,h,c}); }
  void fillCircle(int x,int y,int r,uint16_t c) { if (recordAll) rects.push_back({x-r,y-r,2*r+1,2*r+1,c}); }
  void fillRoundRect(int x,int y,int w,int h,int,uint16_t c) { if (recordAll) rects.push_back({x,y,w,h,c}); }
  void drawLine(int x,int y,int x2,int y2,uint16_t c) { if (recordAll) rects.push_back({std::min(x,x2),std::min(y,y2),std::abs(x2-x)+1,std::abs(y2-y)+1,c}); }
};
