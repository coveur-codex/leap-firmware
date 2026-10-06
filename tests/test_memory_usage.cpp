#include "MemoryUsage.h"
#include <cassert>
using namespace leap;
int main() {
  assert(storage.begin());
  fakeFiles["/memory-test"] = std::make_shared<std::string>(1024, 'x');
  storage.refreshSpace();
  const size_t scans = fakeUsedCalls;
  auto snapshot = memorySnapshot();
  assert(snapshot.flash.used == 2 * 1024 * 1024);
  assert(snapshot.flash.total == 4 * 1024 * 1024);
  assert(snapshot.littlefs.used == 1024);
  assert(snapshot.littlefs.total == 8 * 1024 * 1024);
  assert(snapshot.psram.used == 2 * 1024 * 1024);
  assert(memoryLabel(snapshot.psram) == "2,0 / 8,0 MB");
  JsonDocument request;
  writeMemoryUsage(request, snapshot);
  assert(request["memory"]["littlefs"]["used"] == 1024);
  assert(request["memory"]["flash"]["total"] == 4 * 1024 * 1024);
  ESP.psramFree = 5 * 1024 * 1024;
  assert(memorySnapshot().psram.used == 3 * 1024 * 1024);
  ESP.psramTotal = ESP.psramFree = 0;
  assert(memoryLabel(memorySnapshot().psram) == "nicht verfuegbar");
  storage.ready = false;
  storage.refreshSpace();
  assert(memoryLabel(memorySnapshot().littlefs) == "nicht verfuegbar");
  assert(fakeUsedCalls == scans);
}
