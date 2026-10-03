#include "StorageRecovery.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>

int main() {
  std::vector<uint8_t> flash(0x7e0000, 0xff);
  size_t failAt = flash.size();
  auto read = [&](size_t offset, uint8_t *data, size_t count) {
    assert(offset + count <= flash.size());
    if (offset <= failAt && failAt < offset + count)
      return false;
    memcpy(data, flash.data() + offset, count);
    return true;
  };
  assert(leap::partitionErased(flash.size(), read));
  assert(!leap::partitionErased(0, read));
  // Corrupt superblocks do not authorize erasing surviving files elsewhere.
  for (size_t position : {size_t(0), size_t(8192), flash.size() - 1}) {
    flash[position] = 0;
    assert(!leap::partitionErased(flash.size(), read));
    flash[position] = 0xff;
  }
  // Read failures must never be mistaken for an empty partition.
  for (size_t position : {size_t(0), size_t(8192), flash.size() - 1}) {
    failAt = position;
    assert(!leap::partitionErased(flash.size(), read));
  }
  failAt = flash.size();
  // Include trailing bytes even for sizes that are not a buffer multiple.
  assert(leap::partitionErased(513, read));
  flash[512] = 0;
  assert(!leap::partitionErased(513, read));
  puts("PASS: blank flash initialization guard, surviving data, read failures and trailing bytes");
}
