#include "Storage.h"
#include "JsonLimits.h"
#include <cassert>
#include <iostream>
using namespace leap;
int main() {
  Storage store;
  assert(store.begin());
  JsonDocument state(&jsonRam), initial(&jsonRam), inventory(&jsonRam);
  store.load(initial);
  state["schema"] = 1;
  state["config"]["deviceId"] = "leap-test";
  state["assets"].to<JsonObject>();
  std::string hash(64, 'a');
  fakeFiles["/blobs/" + hash] = std::make_shared<std::string>();
  JsonDocument manifest(&jsonRam);
  manifest["packageId"] = "quiz-test";
  manifest["version"] = 1;
  manifest["type"] = "quiz";
  auto file = manifest["files"].to<JsonArray>().add<JsonObject>();
  file["path"] = "questions.json";
  file["sha256"] = hash;
  file["size"] = 0;
  assert(store.writeJson("/packages/quiz-test-1.json", manifest));
  state["assets"]["quiz-test"] = 1;
  // Reproduce the logged 252,230-byte quiz plus other saved content.
  for (int i = 0; i < 250; ++i)
    state["content"]["quiz"]["questions"][i]["q"] = std::to_string(i) + std::string(1000, 'q');
  state["content"]["news"]["articles"][0]["summary"] = std::string(18429, 'n');
  assert(measureJson(state) > JsonLimit && measureJson(state) < SnapshotJsonLimit);
  int checks = 0;
  fakeWriteHook = [&] {
    JsonDocument visible(&jsonRam);
    bool busy = true;
    assert(store.load(visible, 0, &busy) && !busy);
    assert(visible.as<JsonVariantConst>() == initial.as<JsonVariantConst>());
    ++checks;
  };
  assert(store.commit(state));
  fakeWriteHook = nullptr;
  assert(checks > 100 && fakeMaxWrite <= StorageBlockSize);
  JsonDocument visible(&jsonRam);
  size_t opens = fakeOpens;
  assert(store.load(visible, 0, nullptr, &inventory));
  assert(fakeOpens == opens); // No filesystem IO in a UI reload.
  assert(inventory["quiz-test"]["version"] == 1);
  JsonDocument cachedManifest(&jsonRam);
  assert(store.manifest("quiz-test", 1, cachedManifest) && fakeOpens == opens);
  auto heldView = store.manifestView();
  assert(heldView && (*heldView)["quiz-test"]["version"] == 1);
  std::shared_ptr<const JsonDocument> loadedView;
  assert(store.load(visible, 0, nullptr, nullptr, &loadedView));
  assert(heldView == loadedView && fakeOpens == opens); // Share, no inventory clone.
  assert(visible.as<JsonVariantConst>() == state.as<JsonVariantConst>());
  size_t scans = fakeUsedCalls;
  assert(store.totalSpace.load() == LittleFS.totalBytes());
  assert(store.totalSpace.load() - store.freeBytes() == LittleFS.usedBytes());
  scans = fakeUsedCalls;
  store.freeBytes(); store.freeBytes(); store.totalSpace.load();
  assert(fakeUsedCalls == scans); // No filesystem scan in the health/UI loop.
  Storage reboot;
  assert(reboot.begin() && reboot.load(visible));
  assert(visible.as<JsonVariantConst>() == state.as<JsonVariantConst>());
  uint32_t generation = store.generation;
  JsonDocument oversized(&jsonRam);
  oversized.set(state);
  for (int i = 0; i < 20; ++i)
    oversized["tooLarge"][i] = std::to_string(i) + std::string(60000, 'x');
  assert(!store.commit(oversized) && store.generation == generation);
  fakeRenameFail = true;
  state["config"]["deviceId"] = "uncommitted";
  assert(!store.commit(state) && store.generation == generation);
  assert(store.load(visible) && visible["config"]["deviceId"] == "leap-test");
  fakeRenameFail = false;
  fakeWriteFail = true;
  assert(!store.commit(state));
  fakeWriteFail = false;
  // A >256-KiB catalog is supported without raising limits for control JSON.
  JsonDocument catalog(&jsonRam);
  for (int i = 0; i < 300; ++i)
    catalog["questions"][i]["q"] = std::to_string(i) + std::string(1000, 'q');
  assert(!store.writeJson("/catalog.json", catalog));
  assert(store.writeJson("/catalog.json", catalog, CatalogJsonLimit));
  assert(!store.readJson("/catalog.json", visible));
  assert(store.readJson("/catalog.json", visible, CatalogJsonLimit));
  // Cache keeps working if flash changes; boot must still validate disk.
  fakeFiles["/state1.json"] = std::make_shared<std::string>("broken");
  assert(store.load(visible) && visible["config"]["deviceId"] == "leap-test");
  Storage fallback;
  assert(fallback.begin());
  assert(!fallback.load(visible)); // No second committed snapshot in this fixture.
  assert(visible["schema"] == 1);
  fakeFiles["/state1.json"] = std::make_shared<std::string>();
  serializeJson(visible, *fakeFiles["/state1.json"]);
  state["config"]["deviceId"] = "leap-test";
  assert(store.commit(state)); // Writes state0; state1 remains a valid older fallback.
  assert(store.manifestView() != heldView);
  assert((*heldView)["quiz-test"]["version"] == 1); // Old UI view survives publication.
  fakeFiles["/state0.json"] = std::make_shared<std::string>("broken");
  Storage recovered;
  assert(recovered.begin() && recovered.load(visible));
  assert(visible["schema"] == 1 && Preferences::ints["leap-storeactive"] == 1);
  std::cout << "PASS: large quiz snapshot, RAM reload during chunked writes, limits and failed activation\n";
}
