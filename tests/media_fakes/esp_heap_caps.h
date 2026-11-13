#pragma once
#include <cstdlib>
#include <vector>
constexpr int MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2;
struct FakeMediaAllocation { size_t bytes; int caps; };
inline std::vector<FakeMediaAllocation> fakeMediaAllocations;
inline int fakeMediaFailAt = -1;
inline unsigned fakeMediaCapsFrees = 0;
inline void *heap_caps_calloc(size_t n, size_t size, int caps) {
  auto index = int(fakeMediaAllocations.size());
  fakeMediaAllocations.push_back({n * size, caps});
  return index == fakeMediaFailAt ? nullptr : calloc(n, size);
}
inline void heap_caps_free(void *value) { if (value) { ++fakeMediaCapsFrees; free(value); } }
