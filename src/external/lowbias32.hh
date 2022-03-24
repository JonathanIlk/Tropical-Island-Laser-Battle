#pragma once
#include <cstdint>
// a public-domain low-bias non-cryptographic hash function.
// taken from https://nullprogram.com/blog/2018/07/31/

// exact bias: 0.17353355999581582
inline uint32_t lowbias32(uint32_t x) {
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    x ^= x >> 16;
    return x;
}
// inverse
inline uint32_t lowbias32_r(uint32_t x) {
    x ^= x >> 16;
    x *= UINT32_C(0x43021123);
    x ^= x >> 15 ^ x >> 30;
    x *= UINT32_C(0x1d69e2a5);
    x ^= x >> 16;
    return x;
}
