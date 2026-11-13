#include "Transport.h"
#include "NetworkProfiles.h"
#include "Protocol.h"
#include <cassert>
using namespace leap;
namespace leap {
DeviceSettings deviceSettings;
RamAllocator jsonRam;
Storage storage;
void *RamAllocator::allocate(size_t n) { return malloc(n); }
void RamAllocator::deallocate(void *p) { free(p); }
void *RamAllocator::reallocate(void *p, size_t n) { return realloc(p,n); }
size_t Storage::freeBytes() const { return 8000000; }
bool Storage::parents(const String &) { return true; }
void Storage::refreshSpace() {}
}
int main() {
  Transport transport;
  JsonDocument response, body;
  body["firmwareVersion"] = "1.6.2";
  strcpy(deviceSettings.server, "https://home.example:8443");
  strcpy(deviceSettings.tlsCa, "CA");
  transportReplies.push_back({-1, "", -1, -9984, ECONNREFUSED});
  assert(!transport.json("/api/v1/devices/leap-papa/checkin?secret=token", response, &body));
  assert(transport.status == -1 && transportMethod == "POST");
  assert(transportBody.find("1.6.2") != std::string::npos);
  assert(transport.error.find("connection refused") != std::string::npos);
  assert(transport.error.find("JSON limit") == std::string::npos);
  assert(radioSerial.find("host=home.example port=8443 tls=1") != std::string::npos);
  assert(radioSerial.find("TLS=-9984") != std::string::npos);
  assert(radioSerial.find("errno=") != std::string::npos && radioSerial.find("largest=32000") != std::string::npos);
  assert(radioSerial.find("secret") == std::string::npos && radioSerial.find("token") == std::string::npos);
  assert(radioSerial.find("\\n") == std::string::npos && radioSerial.back() == '\n');
  assert(fakeTlsClientsCreated == 1 && fakeTlsClientsAlive == 0);
  assert(radioTlsError == 0); // end() cleared it, but the diagnostic captured it first.
  radioSerial.clear();
  strcpy(deviceSettings.server, "http://192.0.2.10:8080");
  transportReplies.push_back({200, "{\"ok\":true}"});
  assert(transport.json("/api/v1/devices/leap-papa/checkin", response, &body));
  assert(response["ok"] == true);
  assert(fakeTlsClientsCreated == 1 && fakeTlsClientsAlive == 0); // HTTP never constructs TLS.
  assert(radioSerial.find("host=192.0.2.10 port=8080 tls=0") != std::string::npos);
  assert(radioSerial.find("ok=1") != std::string::npos);
  transportReplies.push_back({403, "", 0});
  assert(!transport.json("/api/v1/test", response));
  assert(transport.error == "HTTP=403; expected 200");
  transportReplies.push_back({200, "", int(JsonLimit) + 1});
  assert(!transport.json("/api/v1/test", response));
  assert(transport.error.find("too large") != std::string::npos);
  transportReplies.push_back({200, "invalid"});
  assert(!transport.json("/api/v1/test", response));
  assert(transport.error.find("JSON parse failed") != std::string::npos);
  assert(!transport.json("/not-api", response));
  assert(transport.error.find("setup failed") != std::string::npos);
  JsonDocument plan, inventory;
  deserializeJson(plan, R"({"syncId":"old","initialFirmware":"1.6.0","firmware":null,"desiredAssets":{"avatar":1}})");
  deserializeJson(inventory, R"({"avatar":1})");
  assert(resumableAssetSync(plan.as<JsonVariantConst>(), inventory.as<JsonVariantConst>(), "1.6.0"));
  assert(!resumableAssetSync(plan.as<JsonVariantConst>(), inventory.as<JsonVariantConst>(), "1.6.2"));
  plan["initialFirmware"] = "1.6.2";
  assert(resumableAssetSync(plan.as<JsonVariantConst>(), inventory.as<JsonVariantConst>(), "1.6.2"));
  plan["firmware"]["version"] = "1.6.3";
  assert(!resumableAssetSync(plan.as<JsonVariantConst>(), inventory.as<JsonVariantConst>(), "1.6.2"));
  plan["firmware"] = nullptr; inventory["avatar"] = 2;
  assert(!resumableAssetSync(plan.as<JsonVariantConst>(), inventory.as<JsonVariantConst>(), "1.6.2"));
  puts("PASS: actual Transport JSON reports TLS/errno/endpoint before close, separates network/HTTP/JSON failures, redacts query strings and resumes asset plans only for the running firmware");
}
