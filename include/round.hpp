#ifndef ROUND_HPP
#define ROUND_HPP

#include <cstdint>

/**
 * @file round.hpp
 * @brief Fungsi F (round function) Feistel berbasis sigma-delta.
 */

/**
 * @brief Fungsi F Feistel: menghasilkan masker 64 bit dari setengah blok dan round key.
 *
 * Dipanggil sekali per round sebagai @c L xor F(R, K[i]). Urutannya:
 * @code
 * (w0, w1) = R                         // dua word 32 bit, w0 = bit atas
 * (k0, k1) = K                         // dua word 32 bit, k0 = bit atas
 * pass maju  : w0 -> w1                (sigma-delta, state dibawa dari w0 ke w1)
 * w0 = rotl(w0, 7);  w1 = rotl(w1, 19)
 * pass mundur: w1 -> w0                (sigma-delta, kunci k0 dan k1 ditukar)
 * hasil = permute_bits(w0 || w1)
 * @endcodestruktur
 *
 * Tiap langkah sigma-delta menyimpan state (akumulator dan feedback), jadi F
 * uninvertable. Karena Feistel tidak butuh invers F:,
 * dekripsi cukup menggunakan F lagi dengan input yang sama.
 *
 * @param[in] r Setengah blok (64 bit).
 * @param[in] k Round key (64 bit).
 * @return Masker 64 bit yang di-XOR ke setengah blok lainnya.
 *
 * @see permute_bits, sbox_bytes32
 */
uint64_t round_f(uint64_t r, uint64_t k);

#endif