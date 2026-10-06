#pragma once
#include "esp_partition.h"
inline const esp_partition_t *esp_ota_get_running_partition() {
  static esp_partition_t partition{4 * 1024 * 1024};
  return &partition;
}
struct FakeEsp {
  size_t psramTotal = 8 * 1024 * 1024, psramFree = 6 * 1024 * 1024;
  size_t getSketchSize() { return 2 * 1024 * 1024; }
  size_t getPsramSize() { return psramTotal; }
  size_t getFreePsram() { return psramFree; }
};
inline FakeEsp ESP;
