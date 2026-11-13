#include "Radio.h"
#include "RadioStream.h"
#include "FirmwareBuild.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <stdexcept>
// Linker bundle stand-in: the TLS fake checks that verified bundle setup is used.
asm(".section .rodata\n.global _binary_x509_crt_bundle_start\n_binary_x509_crt_bundle_start:\n.byte 0\n.global _binary_x509_crt_bundle_end\n_binary_x509_crt_bundle_end:\n.previous");
using namespace leap;
void run() {
  try { radioTask(radioContext); } catch (const WorkerIdle &) {}
  assert(radioAllocations == 0); // Every failure/cancellation frees the MP3/PCM session.
}
void reset() {
  radioSerial.clear(); radioRequestedUrls.clear(); radioResponses.clear();
  radioTlsError = 0; radioDnsWorks = true; radioWifiStatus = WL_CONNECTED;
}
int main() {
  assert(radio.begin());
  const char *url = "https://radio.example/private-token/stream.mp3?secret=123";
  radioResponses.push_back({403, {}, {}});
  radio.play(url);
  assert(radioRequestedUrls.empty()); // UI call queues work, never opens the connection.
  run();
  assert(radio.status() == RadioStatus::HttpError && radio.httpCode() == 403 && !radio.active());
  assert(radioSerial.find("HTTP=403") != std::string::npos);
  assert(radioSerial.find("radio.example") != std::string::npos);
  assert(radioSerial.find("private-token") == std::string::npos && radioSerial.find("secret=123") == std::string::npos);
  assert(radioConnectTimeout == 8000 && radioHeaderTimeout == 8000 && radioHandshakeTimeout == 10 && radioBundleUsed);
  assert(radioUserAgent == std::string("LEAP/") + FirmwareVersion);
  reset(); radioTlsError = -9984;
  radioResponses.push_back({-1, {}, {}}); radio.play(url); run();
  assert(radio.status() == RadioStatus::TlsError);
  assert(radioSerial.find("TLS=-9984") != std::string::npos);
  reset(); radioDnsWorks = false; radio.play(url); run();
  assert(radio.status() == RadioStatus::DnsError && radioRequestedUrls.empty());
  assert(radioSerial.find("DNS failed") != std::string::npos);
  reset(); radioResponses.push_back({-11, {}, {}}); radio.play(url); run();
  assert(radio.status() == RadioStatus::Timeout);
  assert(radioSerial.find("transport=") != std::string::npos);
  reset(); radioResponses.push_back({-8, {}, {}}); radio.play(url); run();
  assert(radio.status() == RadioStatus::MemoryError);
  reset(); radioResponses.push_back({302, {{"Location", "http://other.example/live?token=456"}}, {}});
  radioResponses.push_back({404, {}, {}}); radio.play(url); run();
  assert(radioRequestedUrls.size() == 2 && radioRequestedUrls[1] == "http://other.example/live?token=456");
  assert(radioSerial.find("redirect endpoint=http://other.example") != std::string::npos);
  assert(radioSerial.find("token=456") == std::string::npos);
  assert(radio.httpCode() == 404);
  reset(); radioResponses.push_back({-1, {}, [] { radio.stop(); }}); radio.play(url); run();
  assert(radio.status() == RadioStatus::Stopped && radio.httpCode() == 0);
  reset(); radioResponses.push_back({-1, {}, [] { radio.play("https://radio.example/obsolete"); radio.play("http://radio.example/new"); }});
  radioResponses.push_back({503, {}, {}}); radio.play(url); run();
  assert(radioRequestedUrls.size() == 2 && radioRequestedUrls.back() == "http://radio.example/new");
  assert(radio.status() == RadioStatus::HttpError && radio.httpCode() == 503);
  reset(); radioResponses.push_back({200, {{"Content-Type", "audio/aac"}}, {}}); radio.play(url); run();
  assert(radio.status() == RadioStatus::Unsupported);
  assert(radioSerial.find("unsupported MIME=audio/aac") != std::string::npos);
  reset(); radioWifiStatus = 0; radio.play(url); run();
  assert(radio.status() == RadioStatus::NoWifi && radioRequestedUrls.empty());
  assert(radioHost("https://[2001:db8::1]:8443/live") == "2001:db8::1");
  assert(radioHost("http://radio.example:8000/live") == "radio.example");
  assert(radioHost("https://").empty());
  puts("PASS: actual radio worker logs HTTP/TLS/DNS failures, redacts tokens, uses verified TLS/timeouts, cancels/replaces requests and releases all session buffers");
}
