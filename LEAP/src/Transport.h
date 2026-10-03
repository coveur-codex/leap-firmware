#pragma once
#include "Storage.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>
namespace leap {
class Transport {
public:
  int status = 0;
  String error;
  bool json(const String &path, JsonDocument &response, JsonDocument *body = nullptr);
  bool download(const String &path, size_t size, const String &hash,
                const std::function<bool(const uint8_t *, size_t)> &sink);
  String cacheImage(const String &path, bool radar = false);
  static String encode(const String &value);

private:
  bool open(HTTPClient &http, WiFiClient &plain, WiFiClientSecure &tls, const String &path);
  bool read(HTTPClient &http, size_t size,
            const std::function<bool(const uint8_t *, size_t)> &sink);
};
} // namespace leap
