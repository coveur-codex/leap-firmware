#pragma once
#include <cstddef>
namespace leap {
// Individual HTTP/control documents and the combined offline snapshot have
// different sizes. A valid 250-KiB quiz must not prevent saving weather/news.
constexpr size_t JsonLimit = 256 * 1024;
constexpr size_t CatalogJsonLimit = 1024 * 1024;
constexpr size_t SnapshotJsonLimit = 1024 * 1024;
constexpr size_t StorageBlockSize = 2048;
} // namespace leap
