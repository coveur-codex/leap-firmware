#pragma once
#include <stddef.h>
#include <stdint.h>
struct mbedtls_sha256_context {};
inline void mbedtls_sha256_init(mbedtls_sha256_context *) {}
inline void mbedtls_sha256_free(mbedtls_sha256_context *) {}
inline int mbedtls_sha256_starts(mbedtls_sha256_context *, int) { return 0; }
inline int mbedtls_sha256_update(mbedtls_sha256_context *, const uint8_t *, size_t) { return 0; }
inline int mbedtls_sha256_finish(mbedtls_sha256_context *, uint8_t *out) { for(int i=0;i<32;++i) out[i]=0; return 0; }
