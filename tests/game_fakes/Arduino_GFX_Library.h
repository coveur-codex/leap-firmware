#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
struct Arduino_GFX {
  std::string text;
  bool recordAll = false;
  struct Rect {
    int x, y, w, h;
    uint16_t color;
  };
  std::vector<Rect> rects;
  void fillScreen(uint16_t) {}
  void drawPixel(int x,int y,uint16_t c) { if (recordAll) rects.push_back({x,y,1,1,c}); }
  void drawFastHLine(int x,int y,int w,uint16_t c) { if (recordAll) rects.push_back({x,y,w,1,c}); }
  void setTextColor(uint16_t) {}
  void setTextSize(int) {}
  void setCursor(int, int) {}
  void print(const char *s) {
    text += s;
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
