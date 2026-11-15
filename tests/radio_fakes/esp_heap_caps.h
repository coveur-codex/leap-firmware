#pragma once
#include <Arduino.h>
constexpr unsigned MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2, MALLOC_CAP_INTERNAL = 4;
inline unsigned radioAllocations = 0;
inline size_t radioFailAllocationBytes=0;
inline unsigned radioReadAheadAllocations=0;
inline void *heap_caps_calloc(size_t count, size_t size, unsigned caps) {
  if (size*count==512*1024) {
    if (caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)) abort();
    ++radioReadAheadAllocations;
  }
  if (radioFailAllocationBytes && count*size==radioFailAllocationBytes) return nullptr;
  auto result = calloc(count, size); if (result) ++radioAllocations; return result;
}
inline void heap_caps_free(void *value) { if (value) { --radioAllocations; free(value); } }
inline size_t heap_caps_get_largest_free_block(unsigned) { return 32000; }
