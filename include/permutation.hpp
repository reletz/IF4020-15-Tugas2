#ifndef PERMUTATION_HPP
#define PERMUTATION_HPP

#include <cstdint>

/**
 * @file permutation.hpp
 * @brief Permutasi bit 64 bit (transposisi) untuk layer akhir F.
 */

/// Pengali permutasi. Harus ganjil (relatif prima dengan 64) dan 1 <= m <= 63 agar bijektif.
/// 41 dipilih karena 8 bit dari satu byte tersebar ke 8 byte berbeda.
constexpr int PERM_MULT = 41;

/**
 * @brief Permutasi bit: bit posisi i dipindah ke posisi (i * PERM_MULT) mod 64.
 *
 * @param[in] x Nilai 64 bit.
 * @return Nilai dengan bit-bit yang sudah dipermutasi.
 */
uint64_t permute_bits(uint64_t x);

#endif
