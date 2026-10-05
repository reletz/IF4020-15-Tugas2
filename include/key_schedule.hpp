#ifndef KEY_SCHEDULE_HPP
#define KEY_SCHEDULE_HPP

#include <cstdint>
#include "cipher.hpp"

/**
 * @file key_schedule.hpp
 * @brief Key schedule: membangkitkan round key dari master key.
 */

/**
 * @brief Bangkitkan round key K[0] ... K[15] dari master key.
 *
 * Master key (128 bit) dipecah jadi dua 64 bit (a dan b). Tiap round i:
 * @code
 * a = rotl(a, 13) + PHI * (i + 1)   // mod 2^64
 * a = sbox_bytes(a)
 * b ^= rotl(a, 29)
 * swap(a, b)
 * K_i = a xor b
 * @endcode
 *
 * Konstanta @c PHI (0x9E3779B97F4A7C15, golden ratio) dikali nomor
 * round, jadi tiap round key beda meskipun master key-nya berpola (misal
 * semua nol).
 *
 * @param[in]  a  Separuh kiri master key (8 byte pertama), 64 bit.
 * @param[in]  b  Separuh kanan master key (8 byte terakhir), 64 bit.
 * @param[out] rk Array @ref ROUNDS elemen, diisi K_0 ... K_15.
 *
 * @note Hanya dipanggil sekali per kunci (di constructor @ref CustomCipher),
 *       bukan per blok.
 *
 * Contoh penggunaan:
 * @code
 * uint64_t rk[ROUNDS];
 * key_schedule(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, rk);
 * @endcode
 */
void key_schedule(uint64_t a, uint64_t b, uint64_t rk[ROUNDS]);

#endif
