#pragma once
#include "../input_fakes/Arduino.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdlib>
#include <string>
using SemaphoreHandle_t = void *;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
class String : public std::string {
public:
  using std::string::string;
  String(const std::string &s) : std::string(s) {}
  String(int n) : std::string(std::to_string(n)) {}
  void toLowerCase() {
    for (char &c : *this)
      if (c >= 'A' && c <= 'Z')
        c += 32;
  }
  bool endsWith(const char *s) const {
    auto n = strlen(s);
    return size() >= n && compare(size() - n, n, s) == 0;
  }
};
namespace ArduinoJson {
template <> struct Converter<String> {
  static String fromJson(JsonVariantConst v) {
    return v.as<std::string>();
  }
  static void toJson(const String &s, JsonVariant v) {
    v.set(std::string(s));
  }
  static bool checkJson(JsonVariantConst v) {
    return v.is<const char *>();
  }
};
} // namespace ArduinoJson
template <typename T> T constrain(T value, T lo, T hi) {
  return std::max(lo, std::min(value, hi));
}
inline uint32_t fakeRandom = 0;
inline uint32_t esp_random() {
  return fakeRandom++;
}
struct FakeSerial {
  template <typename... T> void printf(const char *, T...) {}
};
inline FakeSerial Serial;
