#include "Radio.h"
#include "Config.h"
#include "RadioStream.h"
#include "PodcastPlayback.h"
#include "Mp3Metadata.h"
#include "Mp3ReadAhead.h"
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
  std::atomic<bool> ready{false};
  uint64_t played = 0;
};
// The producer detaches the PCM view before any session storage is released,
// including early exits. The UI invalidates generations; it never frees worker memory.
struct Radio::PcmLease {
  Radio &owner;
  Pcm *value;
  PcmLease(Radio &r,Pcm &p):owner(r),value(&p) {
    xSemaphoreTake(owner.mutex,portMAX_DELAY);owner.pcm=value;xSemaphoreGive(owner.mutex);
  }
  void detach() {
    if (!value) return;
    xSemaphoreTake(owner.mutex,portMAX_DELAY);
    if (owner.pcm==value) owner.pcm=nullptr;
    xSemaphoreGive(owner.mutex);
    owner.bufferedMp3=0;owner.bufferedEpoch=0;value=nullptr;
  }
  ~PcmLease() { detach(); }
};
void Radio::clearSession() {
  xSemaphoreTake(mutex,portMAX_DELAY);
  bool stale=pcm!=nullptr;
  pcm=nullptr;
  xSemaphoreGive(mutex);
  bufferedMp3=0;bufferedEpoch=0;
  if (stale) log("RADIO","Stale PCM view detached before new session");
}
bool Radio::begin() {
  mutex = xSemaphoreCreateMutex();
  commands = xQueueCreate(1, sizeof(Command));
  if (mutex && commands && xTaskCreatePinnedToCore([](void *p) { static_cast<Radio *>(p)->run(); },
      "leap-radio", 24576, this, 1, nullptr, 0) == pdPASS) return true;
  if (commands) { vQueueDelete(commands); commands = nullptr; }
  if (mutex) { vSemaphoreDelete(mutex); mutex = nullptr; }
  return false;
}
void Radio::play(const String &url) { queue(url, 0, 0, false, false); }
void Radio::queue(const String &url, uint32_t position, uint32_t duration, bool finite, bool seeked) {
  if (suspended) return;
  Command command{};
  command.position = position; command.duration = duration; command.finite = finite; command.seeked = seeked;
  pausedFlag = false; endedNaturally = false;
  positionSeconds = position; durationSeconds = duration;
  command.generation = generation.fetch_add(1) + 1;
  requestedEpoch = 0; // Invalidate any PCM before publishing the replacement.
  bufferedMp3=0;bufferedEpoch=0;
  if (!commands || !radioUrl(url.c_str())) { state = commands ? RadioStatus::InvalidUrl : RadioStatus::MemoryError; return; }
  strlcpy(command.url, url.c_str(), sizeof(command.url));
  responseCode = 0;
  state = RadioStatus::Connecting;
  requestedEpoch = command.generation;
  xQueueOverwrite(commands, &command);
}
void Radio::stop() {
  pausedFlag = false;
  requestedEpoch = 0;
  bufferedMp3=0;bufferedEpoch=0;
  Command command{};
  command.generation = generation.fetch_add(1) + 1;
  responseCode = 0;
  state = RadioStatus::Stopped;
  if (commands) xQueueOverwrite(commands, &command);
}
size_t Radio::read(int16_t *out, size_t frames, uint32_t &rate) {
  if (!active() || pausedFlag || !mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return 0;
  size_t count = 0;
  if (pcm && pcm->epoch == generation && pcm->ready) {
    auto consumed = pcm->consumed.load();
    count = std::min(frames * 2, size_t(pcm->written.load() - consumed));
    for (size_t i = 0; i < count; ++i) out[i] = pcm->samples.get()[(consumed + i) % PcmSamples];
    pcm->consumed = consumed + count;
    pcm->played += count / 2;
    if (pcm->rate) positionSeconds = pcm->played / pcm->rate;
    rate = pcm->rate;
    if (!count) { pcm->ready = false; status(pcm->epoch, RadioStatus::Buffering); }
  }
  xSemaphoreGive(mutex);
  return count / 2;
}
void Radio::run() {
  Command command{};
  auto index = allocate<Mp3SeekIndex>();
  String indexUrl, validator;
  for (;;) {
    if (xQueueReceive(commands, &command, portMAX_DELAY) == pdTRUE && command.url[0] && current(command.generation)) {
      clearSession();
      if (index) stream(command, *index, indexUrl, validator);
      else status(command.generation, RadioStatus::MemoryError);
      clearSession();
      uint32_t completed = command.generation;
      requestedEpoch.compare_exchange_strong(completed, 0);
    }
  }
}
void Radio::stream(const Command &command, Mp3SeekIndex &index, String &indexUrl, String &validator) {
  if (!command.finite || indexUrl != command.url) {
    index = {}; indexUrl = command.url; validator = "";
  }
  if (!validator.length()) index={};
  auto checkpoint = command.finite && validator.length() ? index.before(command.position) : Mp3SeekIndex::Point{};
  const uint32_t epoch = command.generation;
  Serial.printf("[RADIO] session=%lu start wifi=%d rssi=%d heap=%u largest=%u psram=%u unix=%lld\n",
                (unsigned long)epoch, int(WiFi.status()), int(WiFi.RSSI()),
                unsigned(ESP.getFreeHeap()), unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                unsigned(ESP.getFreePsram()), (long long)time(nullptr));
  if (WiFi.status() != WL_CONNECTED) { log("RADIO", "No WiFi connection"); status(epoch, RadioStatus::NoWifi); return; }
  auto compressed = allocate<uint8_t>(Mp3ReadAhead::Bytes);
  auto input = allocate<uint8_t>(InputBytes);
  auto decoded = allocate<mp3d_sample_t>(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
  auto decoder = allocate<mp3dec_t>();
  Pcm output;
  output.samples = allocate<int16_t>(PcmSamples * sizeof(int16_t));
  output.epoch = epoch;
  if (!compressed || !input || !decoded || !decoder || !output.samples) {
    log("RADIO", "PSRAM allocation failed (512-KiB read-ahead/PCM/input/MP3)");
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
    const char *headers[] = {"Content-Type", "Transfer-Encoding", "icy-metaint", "Location", "Content-Length", "Content-Range", "ETag", "Last-Modified"};
    http.collectHeaders(headers, 8);
    if (!http.begin(*client, url)) { log("RADIO", "HTTP begin rejected URL"); result = RadioStatus::InvalidUrl; break; }
    http.setUserAgent(String("LEAP/") + FirmwareVersion);
    http.addHeader("Icy-MetaData", command.finite ? "0" : "1");
    if (checkpoint.offset) {
      http.addHeader("Range", (String("bytes=") + String(checkpoint.offset) + "-").c_str());
      if (validator.length()) http.addHeader("If-Range", validator.c_str());
    }
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
    if (code == 416 && checkpoint.offset) {
      checkpoint = {}; index = {}; validator = ""; http.end(); continue;
    }
    if (code != 200 && !(command.finite && code == 206)) { result = RadioStatus::HttpError; http.end(); break; }
    Serial.printf("[RADIO] session=%lu HTTP response headers complete\n", (unsigned long)epoch);
    if (code == 206) {
      unsigned long long start=0, end=0, total=0;
      if (sscanf(http.header("Content-Range").c_str(), "bytes %llu-%llu/%llu", &start, &end, &total) != 3 ||
          start != checkpoint.offset || end < start || total <= end) {
        result=RadioStatus::Unsupported; http.end(); break;
      }
    } else if (checkpoint.offset) { // Range ignored or If-Range validator changed.
      checkpoint = {}; index = {};
    }
    String newValidator = http.header("ETag");
    if (!newValidator.length()) newValidator = http.header("Last-Modified");
    if (newValidator.startsWith("W/")) newValidator=http.header("Last-Modified");
    if (validator.length() && newValidator!=validator) {
      index={};
      if (code==206 && checkpoint.offset) { checkpoint={}; validator=""; http.end(); continue; }
    }
    validator = newValidator;
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
    if (command.finite && icy) { result=RadioStatus::Unsupported; http.end(); break; }
    RadioBody body(transfer == "chunked", icy);
    Mp3Metadata metadata(checkpoint.offset==0);
    uint64_t decodedSamples = checkpoint.samples, inputOffset = checkpoint.offset;
    uint64_t targetSamples = 0;
    uint32_t sampleRate = index.rate;
    bool cleanEnd = false, transportEnded = false, decodedEnd = false;
    String lengthHeader = http.header("Content-Length");
    int64_t contentLength = -1;
    if (transfer!="chunked" && lengthHeader.length()) {
      char *tail=nullptr;
      auto length=strtoll(lengthHeader.c_str(), &tail, 10);
      if (tail && !*tail && length>=0) contentLength=length;
    }
    Mp3ReadAhead ahead(compressed.get());
    ahead.clear();
    PcmLease lease(*this,output);
    bufferedMp3=0;bufferedEpoch=epoch;
    Serial.printf("[RADIO] session=%lu clean buffers: mp3=0 pcm=0 input=0 capacity=%u PSRAM bytes\n",
                  (unsigned long)epoch,unsigned(Mp3ReadAhead::Bytes));
    status(epoch, RadioStatus::Buffering);
    size_t used = 0, skipped = 0;
    uint32_t lastFrame = millis(), prefillAt=millis();
    Mp3StreamWatch watch(millis());
    bool prefilling=true;
    RadioStatus sourceResult=RadioStatus::ConnectionError;
    bool ended = false, announced = false;
    size_t receivedBytes = 0, decodedFrames = 0, peakBuffered = 0;
    const char *reason = "peer-closed";
    result = RadioStatus::ConnectionError;
    while (current(epoch)) {
      // Keep reading ahead even while PCM is full/paused. Both stages live on
      // this existing worker; no second decoder or competing I2S task is added.
      uint8_t incoming[1024];
      size_t available=!transportEnded ? std::min({size_t(std::max(0,client->available())),ahead.free(),sizeof(incoming)}) : 0;
      if (contentLength>=0)
        available=std::min(available,size_t(std::max(int64_t(0),contentLength-int64_t(receivedBytes))));
      int received=available ? client->read(incoming,available) : 0;
      if (!current(epoch)) break;
      if (received>0) receivedBytes+=received;
      for (int i=0;i<received;++i) {
        int kind=body.feed(incoming[i]);
        if (kind<0 || kind==2) {
          reason=kind<0 ? "invalid-chunks" : "end-of-body";
          sourceResult=kind<0 ? RadioStatus::Unsupported : RadioStatus::ConnectionError;
          ended=true;break;
        }
        if (kind==1 && !ahead.push(incoming[i])) {
          reason="read-ahead-overflow";sourceResult=RadioStatus::MemoryError;ended=true;break;
        }
      }
      if (!transportEnded && (ended || (!client->connected() && !client->available()) ||
          (contentLength>=0 && receivedBytes>=uint64_t(contentLength)))) {
        transportEnded=true;
        cleanEnd=(ended && !strcmp(reason,"end-of-body")) ||
                 (!ended && contentLength>=0 && receivedBytes==uint64_t(contentLength));
        if (!cleanEnd && WiFi.status()!=WL_CONNECTED) { reason="wifi-lost";sourceResult=RadioStatus::NoWifi; }
      }
      if (prefilling && (transportEnded || ahead.size()>=Mp3ReadAhead::PrefillBytes ||
          (ahead.size() && uint32_t(millis()-prefillAt)>=Mp3ReadAhead::PrefillMaxMs))) prefilling=false;
      uint32_t queued=output.written.load()-output.consumed.load();
      bool paused=pausedFlag.load();
      bool buffered=!prefilling && ((output.ready.load() && queued) || ahead.size());
      if (!transportEnded && watch.networkExpired(millis(),buffered,paused,received>0)) {
        transportEnded=true;cleanEnd=false;prefilling=false;
        reason="body-timeout";
        sourceResult=WiFi.status()==WL_CONNECTED ? RadioStatus::Timeout : RadioStatus::NoWifi;
      }
      peakBuffered=std::max(peakBuffered,ahead.size());
      bufferedMp3=uint32_t(ahead.size());
      if (watch.consumerExpired(millis(),output.consumed.load(),queued,output.ready.load(),paused)) {
        reason="pcm-consumer-stalled";result=RadioStatus::AudioStalled;break;
      }
      if (prefilling || paused || queued>PcmSamples-MINIMP3_MAX_SAMPLES_PER_FRAME) {
        lastFrame=millis();vTaskDelay(pdMS_TO_TICKS(3));continue;
      }
      used+=ahead.read(input.get()+used,InputBytes-used);
      bufferedMp3=uint32_t(ahead.size());
      bool decoderEof=transportEnded && !ahead.size();
      auto prefix=metadata.consume(input.get(),used);
      if (prefix.state==Mp3Metadata::State::Invalid ||
          (contentLength>=0 && metadata.declaredBytes()>uint64_t(contentLength))) {
        reason="invalid-id3-header"; result=RadioStatus::InvalidMetadata; break;
      }
      if (prefix.state==Mp3Metadata::State::Skipped) {
        if (prefix.tagBytes) Serial.printf("[RADIO] session=%lu ID3v%u skip=%lu bytes\n",
            (unsigned long)epoch,unsigned(prefix.version),(unsigned long)prefix.tagBytes);
        used-=prefix.consumed;
        inputOffset+=prefix.consumed;
        memmove(input.get(),input.get()+prefix.consumed,used);
        // Recognized metadata bytes are progress, separate from the bounded scan
        // for an actual MP3 frame. The network idle timeout remains in effect.
        lastFrame=millis();
        if (decoderEof && !used && metadata.pending()) {
          reason="truncated-id3-tag"; result=RadioStatus::InvalidMetadata; break;
        }
        vTaskDelay(pdMS_TO_TICKS(1)); continue;
      }
      if (prefix.state==Mp3Metadata::State::NeedMore && used) {
        if (decoderEof) { reason="truncated-id3-header"; result=RadioStatus::InvalidMetadata; break; }
        if (uint32_t(millis()-lastFrame)>=Mp3StreamWatch::NetworkIdleMs) {
          reason="id3-header-timeout";result=RadioStatus::InvalidMetadata;break;
        }
        vTaskDelay(pdMS_TO_TICKS(2)); continue;
      }
      if ((!decoderEof && received>0 && used>=4096 && uint32_t(millis()-lastFrame)>8000) || skipped>128*1024) { reason = "no-mp3-frames"; result = RadioStatus::Unsupported; break; }
      // A few KB allow minimp3 to distinguish consecutive frames from false sync words.
      if (decoderEof && !used) { decodedEnd=true; break; }
      if (used < 4096 && !decoderEof) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
      mp3dec_frame_info_t info{};
      uint64_t frameOffset = inputOffset;
      int samples = mp3dec_decode_frame(decoder.get(), input.get(), int(used), decoded.get(), &info);
      uint32_t timelineSamples=info.layer==3 && info.hz && info.frame_bytes>info.frame_offset+1 ?
          (((input.get()[info.frame_offset+1]>>3)&3)==3 ? 1152 : 576) : 0;
      if (info.frame_bytes > 0) {
        inputOffset += info.frame_bytes;
        used -= info.frame_bytes;
        memmove(input.get(), input.get() + info.frame_bytes, used);
        skipped += info.frame_bytes;
      } else if (used == InputBytes) { reason = "invalid-mp3-data"; result = RadioStatus::Unsupported; break; }
      if (!samples && decoderEof && !info.frame_bytes) { decodedEnd=true; break; }
      if (!samples && command.finite && checkpoint.offset && timelineSamples) {
        // Frames used to reconstruct the bit reservoir still advance absolute time.
        decodedSamples+=timelineSamples;
        lastFrame=millis(); skipped=0;
      }
      if (samples) {
        if (info.layer != 3 || (info.channels != 1 && info.channels != 2) || info.hz < 8000 || info.hz > 48000 || (output.rate && output.rate != uint32_t(info.hz))) {
          result = RadioStatus::Unsupported; break;
        }
        sampleRate = info.hz;
        targetSamples = uint64_t(command.position)*sampleRate;
        if (command.finite) index.add(frameOffset, decodedSamples, sampleRate);
        uint32_t discard = command.finite && decodedSamples < targetSamples ?
                           uint32_t(std::min(uint64_t(samples), targetSamples-decodedSamples)) : 0;
        decodedSamples += samples;
        if (command.finite && !output.written.load()) output.played = decodedSamples - samples + discard;
        ++decodedFrames;
        lastFrame = millis(); skipped = 0;
        auto written = output.written.load();
        for (int i = int(discard); i < samples; ++i) {
          output.samples.get()[(written + (i-int(discard)) * 2) % PcmSamples] = decoded.get()[i * info.channels];
          output.samples.get()[(written + (i-int(discard)) * 2 + 1) % PcmSamples] = decoded.get()[i * info.channels + info.channels - 1];
        }
        xSemaphoreTake(mutex, portMAX_DELAY);
        output.rate = info.hz;
        output.written = written + (samples-discard) * 2;
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
    if (current(epoch) && decodedEnd && decodedFrames && sampleRate) {
      // Drain every decoded tail, including a prematurely closed connection.
      // A dead I2S consumer must not leave this worker waiting forever.
      output.ready=true;
      while (current(epoch) && output.consumed.load()<output.written.load()) {
        if (watch.consumerExpired(millis(),output.consumed.load(),
            output.written.load()-output.consumed.load(),true,pausedFlag.load())) {
          reason="pcm-consumer-stalled";result=RadioStatus::AudioStalled;break;
        }
        vTaskDelay(pdMS_TO_TICKS(3));
      }
      if (current(epoch) && result!=RadioStatus::AudioStalled) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Allow the final copied PCM block to reach I2S.
        result=sourceResult;
        if (command.finite && transportEnded && cleanEnd) {
          durationSeconds=decodedSamples/sampleRate;
          positionSeconds=durationSeconds.load();
          endedNaturally=podcastNaturalEnd(command.seeked,
              decodedSamples/sampleRate>command.position ? decodedSamples/sampleRate-command.position : 0);
          result=RadioStatus::Ended;
        }
      }
    } else if (decodedEnd) result=sourceResult;
    Serial.printf("[RADIO] session=%lu stream-ended reason=%s bytes=%u frames=%u mp3_peak=%u pcm_queued=%u\n", (unsigned long)epoch,
                  current(epoch) ? reason : "cancelled", unsigned(receivedBytes), unsigned(decodedFrames),
                  unsigned(peakBuffered),unsigned(output.written.load()-output.consumed.load()));
    lease.detach();
    ahead.clear();
    http.end();
    break;
  }
  Serial.printf("[RADIO] session=%lu finished status=%s\n", (unsigned long)epoch, current(epoch) ? label(result) : "cancelled");
  status(epoch, current(epoch) ? result : RadioStatus::Stopped);
  // Network client, decoder and all PSRAM buffers are released here, including cancellation.
}
const char *Radio::label(RadioStatus value) {
  switch (value) {
  case RadioStatus::Paused: return "Pause";
  case RadioStatus::Ended: return "Folge beendet";
  case RadioStatus::Connecting: return "Verbinden …";
  case RadioStatus::Buffering: return "Puffern …";
  case RadioStatus::Playing: return "Wiedergabe";
  case RadioStatus::NoWifi: return "Kein WLAN";
  case RadioStatus::InvalidUrl: return "Stream-URL ungültig";
  case RadioStatus::Unsupported: return "Kein MP3-Stream";
  case RadioStatus::InvalidMetadata: return "MP3-Metadaten ungültig";
  case RadioStatus::AudioStalled: return "Audio-Ausgabe blockiert";
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
