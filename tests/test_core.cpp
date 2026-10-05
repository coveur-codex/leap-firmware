#include "Core.h"
#include "Protocol.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace leap;
int main(int argc, char **argv) {
  assert(utcTimestamp(1970, 1, 1, 0, 0, 0) == 0);
  assert(utcTimestamp(2000, 2, 29, 12, 0, 0) == 951825600);
  assert(meetsVersion("1.0.0", "1.0.0-beta.99"));
  assert(meetsVersion("1.10.0", "1.9.9"));
  assert(!meetsVersion("1.0.0-beta.9", "1.0.0-beta.10"));
  assert(!meetsVersion("01.0.0", "1.0.0"));
  assert(!meetsVersion("1.0.0-rc.1", "1.0.0"));
  assert(elapsed(25, UINT32_MAX - 24, 50));
  assert(!elapsed(25, UINT32_MAX - 24, 51));
  Debouncer b;
  assert(!b.poll(true, 0, false));
  assert(!b.poll(false, 10, false));
  assert(!b.poll(true, 20, false));
  assert(b.poll(true, 45, false) == 1);
  assert(!b.poll(true, 200, false));
  assert(b.poll(true, 945, false) == 3);
  assert(!b.poll(true, 1945, false));
  assert(!b.poll(false, 1950, false));
  assert(!b.poll(false, 1975, false));
  assert(!b.stable);
  Debouncer r;
  assert(!r.poll(true, UINT32_MAX - 30, true));
  assert(r.poll(true, UINT32_MAX - 5, true) == 1);
  assert(r.poll(true, 410, true) == 2);
  for (auto path :
       {"../secret", "/root", "a/../b", "a//b", "a/./b", "a\\b", "a/%2e%2e/b", "a?x", "a/"})
    assert(!safePath(path));
  assert(safePath("animations/idle/001.png"));
  assert(safePath("Bilder/Grüße.png"));
  assert(!identifier("../pkg"));
  ChatPacket p;
  p.templates = 1;
  p.boot = 1;
  p.sequence = 1;
  strcpy(p.message, "12345678-1234-1234-1234-123456789012");
  assert(validPacket(p, sizeof(p)));
  assert(!validPacket(p, sizeof(p) - 1));
  p.protocol = 2;
  assert(!validPacket(p, sizeof(p)));
  p.protocol = 1;
  SeenMessages seen;
  assert(seen.accept(p));
  assert(!seen.accept(p));
  p.boot++;
  assert(seen.accept(p));
  p.sender[0]++;
  assert(seen.accept(p));
  p.name[32] = 'x';
  assert(!validPacket(p, sizeof(p)));
  assert(!requiredAssetFile("avatar", "preview.svg"));
  assert(!requiredAssetFile("avatar", "preview.SVG"));
  assert(requiredAssetFile("avatar", "data/pet/idle/frame_01.png"));
  assert(requiredAssetFile("avatar", "definition.json"));
  JsonDocument petDefinition;
  petDefinition["tamagotchi"]["backgrounds"]["day"] = "outer/background_day/day.png";
  assert(avatarImageSize(petDefinition, "outer/background_day/day.png", 256, 142));
  assert(avatarImageSize(petDefinition, "outer/background_day/day.png", 264, 142));
  assert(!avatarImageSize(petDefinition, "outer/background_day/day.png", 1025, 142));
  assert(avatarImageSize(petDefinition, "idle/frame_01.png", 80, 80));
  assert(!avatarImageSize(petDefinition, "unreferenced.png", 256, 142));
  JsonDocument avatar;
  avatar["definition"]["preview"] = "preview.svg";
  avatar["files"].to<JsonArray>().add<JsonObject>()["path"] = "data/pet/idle/frame_01.png";
  assert(avatarFrame(avatar) == "data/pet/idle/frame_01.png");
  auto frames = avatar["definition"]["animations"]["idle"]["frames"].to<JsonArray>();
  frames.add("data/pet/idle/frame_01.png");
  frames.add("preview.svg");
  assert(avatarFrame(avatar, 1000) == "data/pet/idle/frame_01.png");
  JsonDocument manifest;
  manifest["schemaVersion"] = 1;
  manifest["packageId"] = "avatar-test";
  manifest["version"] = 2;
  manifest["type"] = "avatar";
  manifest["definition"]["id"] = "avatar-test";
  manifest["definition"]["version"] = 2;
  manifest["definition"]["type"] = "avatar";
  auto file = manifest["files"].to<JsonArray>().add<JsonObject>();
  file["path"] = "definition.json";
  file["url"] = "/api/v1/packages/avatar-test/versions/2/files/definition.json";
  file["size"] = 10;
  file["sha256"] = std::string(64, 'a');
  assert(manifestMetadata(manifest, 10));
  assert(!manifestMetadata(manifest, 9));
  file["size"] = -1;
  assert(!manifestMetadata(manifest, 100));
  file["size"] = 10;
  file["url"] = "/api/v1/devices/other/config";
  assert(!manifestMetadata(manifest, 100));
  file["url"] = "/api/v1/packages/avatar-test/versions/2/files/definition.json";
  manifest["files"].add(file);
  assert(!manifestMetadata(manifest, 100));
  manifest["files"].remove(1);
  manifest["schemaVersion"] = 2;
  assert(!manifestMetadata(manifest, 100));
  manifest["schemaVersion"] = 1;
  if (argc > 1) {
    std::ifstream stream(argv[1]);
    JsonDocument fixtures;
    assert(!deserializeJson(fixtures, stream));
    assert(deviceConfig(fixtures["config"], "leap-test", 1));
    for (JsonVariant m : fixtures["manifests"].as<JsonArray>()) {
      assert(manifestMetadata(m, 8 * 1024 * 1024));
      if (m["type"] == "avatar") {
        assert(avatarFrame(m) == "data/pet/idle/frame_01.png");
        assert(avatarPageImage(m, "aircraft") == "data/pet/pagestatics/flightradar.png");
        assert(avatarPageImage(m, "home") == "data/pet/pagestatics/home.png");
        assert(avatarImageSize(m["definition"], "background_day.png", 256, 142));
        assert(avatarImageSize(m["definition"], "background_night.png", 256, 142));
        JsonArray fileRows = m["files"].as<JsonArray>();
        for (JsonObject f : fileRows)
          if (suffix(f["path"] | "", ".svg"))
            assert(!requiredAssetFile("avatar", f["path"] | ""));
      }
    }
  }
  if (argc > 1)
    std::cout << "PASS: real Homeserver fixtures and PNG-only avatar selection\n";
  std::cout << "PASS: debounce, wraparound, UTC, SemVer, radio validation/dedup, traversal and "
               "manifest bounds\n";
}
