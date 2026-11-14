#pragma once
#include <cstddef>
#include <cstring>

// glibc 2.38+ provides strlcpy, including a fortified inline implementation.
// Defining an Arduino stand-in again conflicts with that implementation at -O2.
#if defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2, 38) && defined(__USE_MISC)
#  define LEAP_TEST_HAS_SYSTEM_STRLCPY
# endif
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# define LEAP_TEST_HAS_SYSTEM_STRLCPY
#endif

#ifndef LEAP_TEST_HAS_SYSTEM_STRLCPY
inline std::size_t strlcpy(char *out, const char *in, std::size_t capacity) {
  const std::size_t length = std::strlen(in);
  if (capacity) {
    const std::size_t count = length < capacity ? length : capacity - 1;
    std::memcpy(out, in, count);
    out[count] = '\0';
  }
  return length;
}
#endif
#undef LEAP_TEST_HAS_SYSTEM_STRLCPY
