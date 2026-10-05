#include "PetAssets.h"
#include "Protocol.h"
#include <cassert>
#include <iostream>
using namespace leap;
int main() {
  JsonDocument manifest, presentation;
  manifest["definition"]["animations"]["idle"]["frames"].to<JsonArray>().add("preview.png");
  auto files = manifest["files"].to<JsonArray>();
  files.add<JsonObject>()["path"] = "preview.png";
  for (const char *state : {"idle", "happy", "eating", "sleeping"})
    for (int i = 4; i > 0; --i)
      files.add<JsonObject>()["path"] =
          "Example/data/pet/" + std::string(state) + "/frame_0" + std::to_string(i) + ".png";
  files.add<JsonObject>()["path"] = "Example/background_day/day.png";
  files.add<JsonObject>()["path"] = "Example/background_night.png";
  std::string before;
  serializeJson(manifest, before);
  petPresentation(manifest, presentation);
  assert(presentation["backgrounds"]["day"] == "Example/background_day/day.png");
  assert(presentation["backgrounds"]["night"] == "Example/background_night.png");
  assert(presentation["animations"]["idle"]["frames"].size() == 4);
  assert(presentation["animations"]["idle"]["frames"][0] == "Example/data/pet/idle/frame_01.png");
  assert(presentation["animations"]["happy"]["frames"].size() == 4);
  assert(presentation["animations"]["eating"]["frames"].size() == 4);
  std::string after;
  serializeJson(manifest, after);
  assert(after == before); // No mutation of hashed definitions.
  assert(avatarImageSize(manifest["definition"], "Example/background_day/day.png", 256, 142));
  assert(avatarImageSize(manifest["definition"], "Example/background_day/day.png", 264, 142));
  assert(avatarImageSize(manifest["definition"], "Example/background_night.png", 428, 142));
  assert(!avatarImageSize(manifest["definition"], "Example/background_day/day.png", 1025, 142));
  assert(!avatarImageSize(manifest["definition"], "Example/background_day/day.png", 264, 0));
  assert(!avatarImageSize(manifest["definition"], "preview.png", 264, 142));
  assert(avatarImageSize(manifest["definition"], "preview.png", 80, 80));
  manifest["definition"]["tamagotchi"]["backgrounds"]["day"] = "custom.png";
  manifest["definition"]["tamagotchi"]["animations"]["idle"]["frames"].to<JsonArray>().add(
      "custom-idle.png");
  assert(avatarImageSize(manifest["definition"], "custom.png", 264, 142));
  petPresentation(manifest, presentation);
  assert(presentation["backgrounds"]["day"] == "custom.png");
  assert(presentation["animations"]["idle"]["frames"][0] == "custom-idle.png");
  manifest["definition"].clear();
  assert(avatarFrame(manifest, 0) == "Example/data/pet/idle/frame_01.png");
  assert(avatarFrame(manifest, 400) == "Example/data/pet/idle/frame_02.png");
  assert(avatarFrame(manifest, 800) == "Example/data/pet/idle/frame_03.png");
  // Every page selects the corresponding static in the assigned avatar package.
  for (const char *page : {"home", "news", "weather", "flightradar", "quiz", "games",
                          "communication", "knowledge", "settings"}) {
    std::string path = "data/pet/pagestatics/" + std::string(page) + ".png";
    files.add<JsonObject>()["path"] = path;
    assert(avatarPageImage(manifest, page) == path);
    assert(requiredAssetFile("avatar", path));
    assert(avatarImageSize(manifest["definition"], path, 80, 80));
    assert(!avatarImageSize(manifest["definition"], path, 256, 142));
  }
  assert(avatarPageImage(manifest, "aircraft") == "data/pet/pagestatics/flightradar.png");
  assert(avatarPageImage(manifest, "chill") == "data/pet/pagestatics/home.png");
  assert(avatarPageImage(manifest, "../settings") == "data/pet/pagestatics/home.png");
  JsonDocument oldPackage;
  oldPackage["files"].to<JsonArray>().add<JsonObject>()["path"] =
      "Example/data/pet/idle/frame_01.png";
  assert(avatarPageImage(oldPackage, "aircraft") == "Example/data/pet/idle/frame_01.png");
  auto oldFiles = oldPackage["files"].as<JsonArray>();
  oldFiles.add<JsonObject>()["path"] = "files/data/pet/pagestatics/home.png";
  assert(avatarPageImage(oldPackage, "news") == "files/data/pet/pagestatics/home.png");
  oldFiles.add<JsonObject>()["path"] = "Example/data/pet/pagestatics/flightradar.png";
  assert(avatarPageImage(oldPackage, "aircraft") ==
         "Example/data/pet/pagestatics/flightradar.png");
  oldFiles.add<JsonObject>()["path"] = "data/pet/pagestatics/flightradar.png";
  assert(avatarPageImage(oldPackage, "aircraft") == "data/pet/pagestatics/flightradar.png");
  assert(avatarFrame(manifest, 800) == "Example/data/pet/idle/frame_03.png");
  std::cout << "PASS: page statics, flight radar alias, prefixed paths and static legacy fallback\n";
  std::cout << "PASS: old manifests discover backgrounds and four state frames without altering "
               "metadata\n";
}
