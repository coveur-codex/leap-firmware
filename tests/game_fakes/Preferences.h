#pragma once
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
  std::string space;

public:
  inline static std::map<std::string, std::vector<uint8_t>> bytes;
  inline static std::map<std::string, int> ints;
  inline static bool failWrites = false;
  inline static std::map<std::string, unsigned> writes;
  bool begin(const char *name, bool) {
    space = name;
    return true;
  }
  size_t getBytesLength(const char *key) {
    return bytes[space + key].size();
  }
  size_t getBytes(const char *key, void *dest, size_t n) {
    auto &data = bytes[space + key];
    if (data.size() != n)
      return 0;
    memcpy(dest, data.data(), n);
    return n;
  }
  size_t putBytes(const char *key, const void *source, size_t n) {
    ++writes[space + key];
    if (failWrites) return 0;
    auto p = static_cast<const uint8_t *>(source);
    bytes[space + key] = {p, p + n};
    return n;
  }
  unsigned char getUChar(const char *key, unsigned char fallback) {
    return static_cast<unsigned char>(getInt(key, fallback));
  }
  size_t putUChar(const char *key, unsigned char value) {
    ints[space + key] = value;
    return 1;
  }
  int getInt(const char *key, int fallback) {
    auto p = ints.find(space + key);
    return p == ints.end() ? fallback : p->second;
  }
  size_t putInt(const char *key, int value) {
    ints[space + key] = value;
    return sizeof(value);
  }
};
