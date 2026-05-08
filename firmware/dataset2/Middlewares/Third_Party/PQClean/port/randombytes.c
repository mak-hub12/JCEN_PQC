#include <stdint.h>
#include <stddef.h>
#include "../common/randombytes.h"

/* Deterministic, fast PRNG for benchmarking only (NOT CRYPTO-SECURE).
   Xoshiro128**  (public-domain). */

static uint32_t s[4] = { 0x243F6A88, 0x85A308D3, 0x13198A2E, 0x03707344 };

static inline uint32_t rotl32(uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

static uint32_t xoshiro128ss(void) {
    const uint32_t result = rotl32(s[1] * 5u, 7) * 9u;
    const uint32_t t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rotl32(s[3], 11);
    return result;
}

/* Optional: call once to vary the seed per boot if you want */
void randombytes_seed(uint32_t seed) {
    s[0] = 0x243F6A88 ^ seed;
    s[1] = 0x85A308D3 ^ (seed<<7);
    s[2] = 0x13198A2E ^ (seed>>3);
    s[3] = 0x03707344 ^ (seed*0x9E3779B1u);
}

/* PQClean wants: int randombytes(uint8_t *out, size_t outlen) returning 0 on success */
int randombytes(uint8_t *out, size_t outlen) {
    for (size_t i = 0; i < outlen; i++) {
        if ((i & 3u) == 0) {
            uint32_t r = xoshiro128ss();
            out[i] = (uint8_t) (r & 0xFFu);
        } else {
            out[i] = (uint8_t) xoshiro128ss();
        }
    }
    return 0;
}
