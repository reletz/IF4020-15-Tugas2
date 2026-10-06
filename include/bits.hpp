#ifndef BITS_HPP
#define BITS_HPP

#include <cstdint>

/// Rotasi kiri 64 bit. n harus 1..63 (n = 0 atau 64 -> undefined behavior).
inline uint64_t rotl64(uint64_t x, unsigned n){
    return (x << n) | x >> (64 - n);
}


/// Rotasi kiri 32 bit. n harus 1..31 (n = 0 atau 32 -> undefined behavior).
inline uint32_t rotl32(uint32_t x, unsigned n){
    return (x << n) | x >> (32 - n);
}

#endif