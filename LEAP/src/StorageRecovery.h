#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace leap {
// Only an entirely erased partition is safe to initialize without user action.
// Checking just the superblocks could destroy recoverable files further on.
template <typename Reader> bool partitionErased(size_t size, Reader read) {
  if (!size)
    return false;
  uint8_t buffer[512];
  for (size_t offset = 0; offset < size;) {
    size_t count = std::min(sizeof(buffer), size - offset);
    if (!read(offset, buffer, count))
      return false;
    for (size_t i = 0; i < count; ++i)
      if (buffer[i] != 0xff)
        return false;
    offset += count;
  }
  return true;
}
} // namespace leap
