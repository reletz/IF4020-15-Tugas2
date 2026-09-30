#ifndef ROUND_HPP
#define ROUND_HPP

#include <cstdint>

/// Fungsi F Feistel: menerima setengah blok (64 bit) dan round key (64 bit).
uint64_t round_f(uint64_t r, uint64_t k);

#endif