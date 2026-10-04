#include "Transport.h"
#include "Core.h"
#include <mbedtls/sha256.h>
namespace leap {
String Transport::encode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (unsigned char c : s)
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      out += char(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  return out;
}
bool Transport::open(HTTPClient &http, WiFiClient &plain, WiFiClientSecure &tls,
                     const String &path) {
  status = 0;
  if (!path.startsWith("/api/") || path.indexOf("..") >= 0 || path.indexOf('\r') >= 0 ||
      path.indexOf('\n') >= 0 || path.indexOf('\\') >= 0)
    return false;
  String base = LEAP_SERVER;
  while (base.endsWith("/"))
    base.remove(base.length() - 1);
  http.setConnectTimeout(HttpTimeout);
  http.setTimeout(HttpTimeout);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  if (base.startsWith("https://")) {
    if (!strlen(LEAP_TLS_CA))
      return false;
    tls.setCACert(LEAP_TLS_CA);
    return http.begin(tls, base + path);
  }
  return base.startsWith("http://") && http.begin(plain, base + path);
}
bool Transport::read(HTTPClient &http, size_t size,
                     const std::function<bool(const uint8_t *, size_t)> &sink) {
  error = "";
  auto *stream = http.getStreamPtr();
  uint8_t buffer[2048];
  size_t done = 0;
  uint32_t last = millis(), start = millis();
  while (done < size) {
    if (elapsed(millis(), last, HttpTimeout) || elapsed(millis(), start, 180000)) {
      error = "Response timeout after " + String(done) + "/" + String(size) + " bytes";
      return false;
    }
    size_t available = stream->available();
    if (available) {
      int n = stream->read(buffer, std::min(sizeof(buffer), std::min(available, size - done)));
      if (n <= 0) {
        error = "Stream read failed after " + String(done) + " bytes";
        return false;
      }
      if (!sink(buffer, n)) {
        error = "Data sink/write rejected block at " + String(done) + " bytes";
        return false;
      }
      done += n;
      last = millis();
    } else if (!http.connected()) {
      error = "Connection closed after " + String(done) + "/" + String(size) + " bytes";
      return false;
    }
    vTaskDelay(1);
  }
  return true;
}
bool Transport::json(const String &path, JsonDocument &response, JsonDocument *body) {
  error = "";
  WiFiClient plain;
  WiFiClientSecure tls;
  HTTPClient http;
  if (!open(http, plain, tls, path)) {
    error = "HTTP setup failed";
    log("HTTP", error.c_str());
    return false;
  }
  if (body) {
    String raw;
    serializeJson(*body, raw);
    http.addHeader("Content-Type", "application/json");
    status = http.POST(raw);
  } else
    status = http.GET();
  int size = http.getSize();
  bool ok = status == 200 && size > 0 && size <= int(JsonLimit);
  if (!ok)
    error = "HTTP=" + String(status) + " bytes=" + String(size) + " JSON limit=" + String(JsonLimit);
  char *data = ok ? static_cast<char *>(jsonRam.allocate(size + 1)) : nullptr;
  if (!data) {
    if (ok) error = "Cannot allocate HTTP JSON buffer";
    ok = false;
  }
  size_t pos = 0;
  if (ok)
    ok = read(http, size, [&](const uint8_t *p, size_t n) {
      memcpy(data + pos, p, n);
      pos += n;
      return true;
    });
  if (ok) {
    data[size] = 0;
    auto parsed = deserializeJson(response, static_cast<const char *>(data), size);
    ok = !parsed && !response.overflowed();
    if (!ok) error = "JSON parse failed: " + String(parsed.c_str());
  }
  if (data)
    jsonRam.deallocate(data);
  http.end();
  Serial.printf("[%10lu] [HTTP    ] %s status=%d bytes=%d ok=%d\n", millis(), body ? "POST" : "GET",
                status, size, ok);
  if (!ok)
    Serial.printf("[HTTP] path=%s reason=%s\n", path.c_str(), error.c_str());
  return ok;
}
bool Transport::download(const String &path, size_t size, const String &hash,
                         const std::function<bool(const uint8_t *, size_t)> &sink) {
  error = "";
  if (!size || !digestValid(hash.c_str())) {
    error = "Invalid expected size or SHA-256";
    return false;
  }
  WiFiClient plain;
  WiFiClientSecure tls;
  HTTPClient http;
  if (!open(http, plain, tls, path)) {
    error = "HTTP setup failed (path, server URL or TLS CA)";
    return false;
  }
  status = http.GET();
  bool ok = status == 200 && http.getSize() == int(size);
  if (!ok)
    error = "HTTP=" + String(status) + " Content-Length=" + String(http.getSize()) +
            " expected=" + String(size);
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  if (ok)
    ok = read(http, size, [&](const uint8_t *p, size_t n) {
      mbedtls_sha256_update(&ctx, p, n);
      return sink(p, n);
    });
  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  http.end();
  char encoded[65];
  for (int i = 0; i < 32; i++)
    snprintf(encoded + 2 * i, 3, "%02x", digest[i]);
  if (ok && hash != encoded) {
    error = "Downloaded SHA-256 mismatch";
    ok = false;
  }
  if (!ok)
    log("DOWNLOAD", error.c_str());
  return ok;
}
String Transport::cacheImage(const String &path, bool radar) {
  WiFiClient plain;
  WiFiClientSecure tls;
  HTTPClient http;
  if (!open(http, plain, tls, path))
    return "";
  status = http.GET();
  int length = http.getSize();
  if (status != 200 || length < 1 || length > 128 * 1024 ||
      storage.freeBytes() < size_t(length) + ReserveBytes) {
    http.end();
    return "";
  }
  File file = LittleFS.open("/image.part", "w");
  if (!file) {
    http.end();
    return "";
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  bool ok = read(http, length, [&](const uint8_t *p, size_t n) {
    mbedtls_sha256_update(&ctx, p, n);
    return file.write(p, n) == n;
  });
  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  http.end();
  file.flush();
  file.close();
  storage.refreshSpace();
  char hash[65];
  for (int i = 0; i < 32; i++)
    snprintf(hash + 2 * i, 3, "%02x", digest[i]);
  String target = radar ? String("/radar/") + hash : storage.blob(hash);
  if (!ok || !storage.parents(target) || !LittleFS.rename("/image.part", target)) {
    LittleFS.remove("/image.part");
    return "";
  }
  return hash;
}

} // namespace leap
