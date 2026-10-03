#pragma once
#include "Arduino.h"
#include <cstdio>
#include <memory>
class File {
  std::shared_ptr<FILE> file;

public:
  File() = default;
  explicit File(const char *path)
      : file(fopen(path, "rb"), [](FILE *p) {
          if (p)
            fclose(p);
        }) {}
  explicit operator bool() const {
    return bool(file);
  }
  size_t size() {
    auto at = ftell(file.get());
    fseek(file.get(), 0, SEEK_END);
    auto n = ftell(file.get());
    fseek(file.get(), at, SEEK_SET);
    return n;
  }
  size_t read(uint8_t *p, size_t n) {
    return file ? fread(p, 1, n, file.get()) : 0;
  }
  bool seek(int32_t at) {
    return file && fseek(file.get(), at, SEEK_SET) == 0;
  }
  void close() {
    file.reset();
  }
};
struct FakeLittleFS {
  File open(const String &path, const char *) {
    return File(path.c_str());
  }
};
inline FakeLittleFS LittleFS;
