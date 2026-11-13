#include "Radio.h"
#include "Config.h"
#include "RadioStream.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <memory>
#include <time.h>
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3/minimp3.h"

// Arduino-ESP32/ESP-IDF's existing Mozilla root bundle, with hostname verification.
extern const uint8_t radioBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t radioBundleEnd[] asm("_binary_x509_crt_bundle_end");
namespace leap {
Radio radio;
namespace radio_detail {
constexpr size_t PcmSamples = 65536; // 128 KiB PSRAM, ~743 ms stereo at 44.1 kHz.
constexpr size_t InputBytes = 16384;
constexpr int32_t ConnectTimeoutMs = 8000;
constexpr uint16_t HeaderTimeoutMs = 8000;
constexpr unsigned HandshakeTimeoutSeconds = 10;
struct FreeHeap { void operator()(void *p) const { heap_caps_free(p); } };
template<class T> using Heap = std::unique_ptr<T, FreeHeap>;
template<class T> Heap<T> allocate(size_t bytes = sizeof(T)) {
  return Heap<T>(static_cast<T *>(heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
}
} // namespace radio_detail
using namespace radio_detail;
struct Radio::Pcm {
  Heap<int16_t> samples;
  uint32_t epoch = 0, rate = 0;
  std::atomic<uint32_t> written{0}, consumed{0};
  bool ready = false;
};
bool Radio::begin() {
  mutex = xSemaphoreCreateMutex();
  commands = xQueueCreate(1, sizeof(Command));
  if (mutex && commands && xTaskCreatePinnedToCore([](void *p) { static_cast<Radio *>(p)->run(); },
      "leap-radio", 24576, this, 1, nullptr, 0) == pdPASS) return true;
  if (commands) { vQueueDelete(commands); commands = nullptr; }
  if (mutex) { vSemaphoreDelete(mutex); mutex = nullptr; }
  return false;
}
void Radio::play(const String &url) {
  if (suspended) return;
  Command command{};
  command.generation = generation.fetch_add(1) + 1;
  requestedEpoch = 0; // Invalidate any PCM before publishing the replacement.
  if (!commands || !radioUrl(url.c_str())) { state = commands ? RadioStatus::InvalidUrl : RadioStatus::MemoryError; return; }
  strlcpy(command.url, url.c_str(), sizeof(command.url));
  responseCode = 0;
  state = RadioStatus::Connecting;
  requestedEpoch = command.generation;
  xQueueOverwrite(commands, &command);
}
void Radio::stop() {
  requestedEpoch = 0;
  Command command{};
  command.generation = generation.fetch_add(1) + 1;
  responseCode = 0;
  state = RadioStatus::Stopped;
  if (commands) xQueueOverwrite(commands, &command);
}
size_t Radio::read(int16_t *out, size_t frames, uint32_t &rate) {
  if (!active() || !mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return 0;
  size_t count = 0;
  if (pcm && pcm->epoch == generation && pcm->ready) {
    auto consumed = pcm->consumed.load();
    count = std::min(frames * 2, size_t(pcm->written.load() - consumed));
    for (size_t i = 0; i < count; ++i) out[i] = pcm->samples.get()[(consumed + i) % PcmSamples];
    pcm->consumed = consumed + count;
    rate = pcm->rate;
    if (!count) { pcm->ready = false; status(pcm->epoch, RadioStatus::Buffering); }
  }
  xSemaphoreGive(mutex);
  return count / 2;
}
void Radio::run() {
  Command command{};
  for (;;) {
    if (xQueueReceive(commands, &command, portMAX_DELAY) == pdTRUE && command.url[0] && current(command.generation)) {
      stream(command);
      uint32_t completed = command.generation;
      requestedEpoch.compare_exchange_strong(completed, 0);
    }
  }
}
void Radio::stream(const Command &command) {
  const uint32_t epoch = command.generation;
  Serial.printf("[RADIO] session=%lu start wifi=%d rssi=%d heap=%u largest=%u psram=%u unix=%lld\n",
                (unsigned long)epoch, int(WiFi.status()), int(WiFi.RSSI()),
                unsigned(ESP.getFreeHeap()), unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                unsigned(ESP.getFreePsram()), (long long)time(nullptr));
  if (WiFi.status() != WL_CONNECTED) { log("RADIO", "No WiFi connection"); status(epoch, RadioStatus::NoWifi); return; }
  auto input = allocate<uint8_t>(InputBytes);
  auto decoded = allocate<mp3d_sample_t>(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
  auto decoder = allocate<mp3dec_t>();
  Pcm output;
  output.samples = allocate<int16_t>(PcmSamples * sizeof(int16_t));
  output.epoch = epoch;
  if (!input || !decoded || !decoder || !output.samples) {
    log("RADIO", "PSRAM allocation failed (PCM/input/MP3)");
    status(epoch, RadioStatus::MemoryError); return;
  }
  mp3dec_init(decoder.get());
  String url(command.url);
  RadioStatus result = RadioStatus::ConnectionError;
  for (int redirects = 0; redirects <= 5 && current(epoch); ++redirects) {
    auto endpoint = radioEndpoint(url.c_str());
    auto host = radioHost(url.c_str());
    uint32_t started = millis();
    Serial.printf("[RADIO] session=%lu request=%d endpoint=%s\n", (unsigned long)epoch, redirects + 1, endpoint.c_str());
    IPAddress address;
    if (!Network.hostByName(host.c_str(), address)) {
      Serial.printf("[RADIO] session=%lu DNS failed host=%s elapsed=%lu ms\n", (unsigned long)epoch, host.c_str(), (unsigned long)(millis() - started));
      result = RadioStatus::DnsError; break;
    }
    Serial.printf("[RADIO] session=%lu DNS address=%s elapsed=%lu ms\n", (unsigned long)epoch, address.toString().c_str(), (unsigned long)(millis() - started));
    if (!current(epoch)) break;
    std::unique_ptr<NetworkClient> client;
    NetworkClientSecure *tls = nullptr;
    if (url.startsWith("https://")) {
      auto secure = new (std::nothrow) NetworkClientSecure;
      if (!secure) { result = RadioStatus::MemoryError; break; }
      secure->setCACertBundle(radioBundleStart, radioBundleEnd - radioBundleStart);
      secure->setHandshakeTimeout(HandshakeTimeoutSeconds);
      tls = secure;
      client.reset(secure);
    } else client.reset(new (std::nothrow) NetworkClient);
    if (!client) { result = RadioStatus::MemoryError; break; }
    HTTPClient http;
    http.setConnectTimeout(ConnectTimeoutMs);
    http.setTimeout(HeaderTimeoutMs);
    http.useHTTP10(true);
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char *headers[] = {"Content-Type", "Transfer-Encoding", "icy-metaint", "Location"};
    http.collectHeaders(headers, 4);
    if (!http.begin(*client, url)) { log("RADIO", "HTTP begin rejected URL"); result = RadioStatus::InvalidUrl; break; }
    http.setUserAgent(String("LEAP/") + FirmwareVersion);
    http.addHeader("Icy-MetaData", "1");
    int code = http.GET();
    if (!current(epoch)) {
      Serial.printf("[RADIO] session=%lu request cancelled\n", (unsigned long)epoch);
      http.end(); break;
    }
    responseCode = code;
    Serial.printf("[RADIO] session=%lu HTTP=%d elapsed=%lu ms heap=%u\n", (unsigned long)epoch, code, (unsigned long)(millis() - started), unsigned(ESP.getFreeHeap()));
    if (code < 0) {
      Serial.printf("[RADIO] session=%lu transport=%s\n", (unsigned long)epoch, HTTPClient::errorToString(code).c_str());
      char detail[160]{};
      int error = tls ? tls->lastError(detail, sizeof(detail)) : 0;
      if (error) {
        Serial.printf("[RADIO] session=%lu TLS=%d detail=%s unix=%lld\n", (unsigned long)epoch, error, detail, (long long)time(nullptr));
        result = RadioStatus::TlsError;
      } else if (code == HTTPC_ERROR_READ_TIMEOUT) result = RadioStatus::Timeout;
      else if (code == HTTPC_ERROR_TOO_LESS_RAM) result = RadioStatus::MemoryError;
      http.end(); break;
    }
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      auto next = radioRedirect(url.c_str(), http.header("Location").c_str());
      http.end();
      if (redirects == 5 || !radioUrl(next)) { log("RADIO", "Invalid redirect or redirect limit"); result = RadioStatus::InvalidUrl; break; }
      Serial.printf("[RADIO] session=%lu redirect endpoint=%s\n", (unsigned long)epoch, radioEndpoint(next).c_str());
      url = next.c_str(); continue;
    }
    if (code != 200) { result = RadioStatus::HttpError; http.end(); break; }
    Serial.printf("[RADIO] session=%lu HTTP response headers complete\n", (unsigned long)epoch);
    String type = http.header("Content-Type"); type.toLowerCase();
    type = type.substring(0, type.indexOf(';') >= 0 ? type.indexOf(';') : type.length()); type.trim();
    if (type.length() && type != "audio/mpeg" && type != "audio/mp3" && type != "audio/x-mpeg" && type != "application/octet-stream") {
      Serial.printf("[RADIO] session=%lu unsupported MIME=%.64s\n", (unsigned long)epoch, type.c_str());
      result = RadioStatus::Unsupported; http.end(); break;
    }
    String transfer = http.header("Transfer-Encoding"); transfer.toLowerCase(); transfer.trim();
    if (transfer.length() && transfer != "chunked" && transfer != "identity") { result = RadioStatus::Unsupported; http.end(); break; }
    String meta = http.header("icy-metaint");
    uint32_t icy = 0;
    bool validMeta = true;
    for (size_t i = 0; i < meta.length(); ++i) {
      if (meta[i] < '0' || meta[i] > '9' || icy > 16 * 1024 * 1024 / 10) { validMeta = false; break; }
      icy = icy * 10 + meta[i] - '0';
    }
    if (!validMeta) { log("RADIO", "Invalid icy-metaint header"); result = RadioStatus::Unsupported; http.end(); break; }
    RadioBody body(transfer == "chunked", icy);
    xSemaphoreTake(mutex, portMAX_DELAY); pcm = &output; xSemaphoreGive(mutex);
    status(epoch, RadioStatus::Buffering);
    size_t used = 0, skipped = 0;
    uint32_t lastByte = millis(), lastFrame = millis();
    bool ended = false, announced = false;
    size_t receivedBytes = 0, decodedFrames = 0;
    const char *reason = "peer-closed";
    result = RadioStatus::ConnectionError;
    while (current(epoch)) {
      if (WiFi.status() != WL_CONNECTED) { reason = "wifi-lost"; result = RadioStatus::NoWifi; break; }
      if (output.written.load() - output.consumed.load() > PcmSamples - MINIMP3_MAX_SAMPLES_PER_FRAME) {
        vTaskDelay(pdMS_TO_TICKS(3)); continue;
      }
      uint8_t incoming[1024];
      size_t available = std::min({size_t(std::max(0, client->available())), InputBytes - used, sizeof(incoming)});
      int received = available ? client->read(incoming, available) : 0;
      if (received > 0) { lastByte = millis(); receivedBytes += received; }
      // Bounded batch; framing and metadata never enter the MP3 decoder.
      for (int i = 0; i < received; ++i) {
        int kind = body.feed(incoming[i]);
        if (kind < 0 || kind == 2) { reason = kind < 0 ? "invalid-chunks" : "end-of-body"; ended = true; break; }
        if (kind == 1) input.get()[used++] = incoming[i];
      }
      if (ended || (!client->connected() && !client->available())) break;
      if (uint32_t(millis() - lastByte) > 8000) { reason = "body-timeout"; result = RadioStatus::Timeout; break; }
      if (uint32_t(millis() - lastFrame) > 8000 || skipped > 128 * 1024) { reason = "no-mp3-frames"; result = RadioStatus::Unsupported; break; }
      // A few KB allow minimp3 to distinguish consecutive frames from false sync words.
      if (used < 4096) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
      mp3dec_frame_info_t info{};
      int samples = mp3dec_decode_frame(decoder.get(), input.get(), int(used), decoded.get(), &info);
      if (info.frame_bytes > 0) {
        used -= info.frame_bytes;
        memmove(input.get(), input.get() + info.frame_bytes, used);
        skipped += info.frame_bytes;
      } else if (used == InputBytes) { reason = "invalid-mp3-data"; result = RadioStatus::Unsupported; break; }
      if (samples) {
        if (info.layer != 3 || (info.channels != 1 && info.channels != 2) || info.hz < 8000 || info.hz > 48000 || (output.rate && output.rate != uint32_t(info.hz))) {
          result = RadioStatus::Unsupported; break;
        }
        ++decodedFrames;
        lastFrame = millis(); skipped = 0;
        auto written = output.written.load();
        for (int i = 0; i < samples; ++i) {
          output.samples.get()[(written + i * 2) % PcmSamples] = decoded.get()[i * info.channels];
          output.samples.get()[(written + i * 2 + 1) % PcmSamples] = decoded.get()[i * info.channels + info.channels - 1];
        }
        xSemaphoreTake(mutex, portMAX_DELAY);
        output.rate = info.hz;
        output.written = written + samples * 2;
        if (output.written.load() - output.consumed.load() >= PcmSamples / 2) {
          output.ready = true; status(epoch, RadioStatus::Playing);
          if (!announced) {
            announced = true;
            Serial.printf("[RADIO] session=%lu playing rate=%d channels=%d bitrate=%d kbps stack_free=%u\n", (unsigned long)epoch, info.hz, info.channels, info.bitrate_kbps, unsigned(uxTaskGetStackHighWaterMark(nullptr)));
          }
        }
        xSemaphoreGive(mutex);
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    Serial.printf("[RADIO] session=%lu stream-ended reason=%s bytes=%u frames=%u\n", (unsigned long)epoch,
                  current(epoch) ? reason : "cancelled", unsigned(receivedBytes), unsigned(decodedFrames));
    xSemaphoreTake(mutex, portMAX_DELAY); pcm = nullptr; xSemaphoreGive(mutex);
    http.end();
    break;
  }
  Serial.printf("[RADIO] session=%lu finished status=%s\n", (unsigned long)epoch, current(epoch) ? label(result) : "cancelled");
  status(epoch, current(epoch) ? result : RadioStatus::Stopped);
  // Network client, decoder and all PSRAM buffers are released here, including cancellation.
}
const char *Radio::label(RadioStatus value) {
  switch (value) {
  case RadioStatus::Connecting: return "Verbinden …";
  case RadioStatus::Buffering: return "Puffern …";
  case RadioStatus::Playing: return "Wiedergabe";
  case RadioStatus::NoWifi: return "Kein WLAN";
  case RadioStatus::InvalidUrl: return "Stream-URL ungültig";
  case RadioStatus::Unsupported: return "Kein MP3-Stream";
  case RadioStatus::DnsError: return "DNS fehlgeschlagen";
  case RadioStatus::TlsError: return "TLS-Verbindung fehlgeschlagen";
  case RadioStatus::HttpError: return "HTTP-Fehler";
  case RadioStatus::ConnectionError: return "Sender nicht erreichbar";
  case RadioStatus::Timeout: return "Stream unterbrochen";
  case RadioStatus::MemoryError: return "Speicher fehlt";
  default: return "Gestoppt";
  }
}
} // namespace leap
