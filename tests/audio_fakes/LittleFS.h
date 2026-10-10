#pragma once
#include "Arduino.h"
#include <memory>
#include <cstdio>
class File {
  std::shared_ptr<FILE> file;
public:
  File() = default;
  explicit File(const char *path) : file(fopen(path,"rb"),[](FILE *p){ if(p) fclose(p); }) {}
  explicit operator bool() const { return bool(file); }
  size_t position() const { return ftell(file.get()); }
  size_t size() const { auto at=position(); fseek(file.get(),0,SEEK_END); auto n=position(); fseek(file.get(),at,SEEK_SET); return n; }
  size_t available() const { return size()-position(); }
  size_t read(uint8_t *p, size_t n) { return file ? fread(p,1,n,file.get()) : 0; }
  bool seek(size_t at) { return file && !fseek(file.get(),at,SEEK_SET); }
  void close() { file.reset(); }
};
struct FakeFS { File open(const String &path, const char * = "r") { return File(path.c_str()); } };
inline FakeFS LittleFS;
