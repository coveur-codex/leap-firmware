#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
struct Arduino_GFX {
  std::string text;
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
  void fillRect(int, int, int, int, uint16_t) {}
  void drawRect(int, int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
};
