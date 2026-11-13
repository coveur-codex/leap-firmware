#pragma once
#include <Arduino.h>
class File {
public:
  explicit operator bool() const { return false; }
  size_t write(const uint8_t *, size_t size) { return size; }
  void flush() {}
  void close() {}
};
struct FakeFs {
  File open(const String &, const char *) { return {}; }
  bool remove(const String &) { return true; }
  bool rename(const String &, const String &) { return true; }
};
inline FakeFs LittleFS;
