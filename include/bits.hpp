#ifndef BITS_HPP
#define BITS_HPP

#include <cstdint>

/**
 * @file bits.hpp
 * @brief Helper operasi bit (rotasi) yang dipakai key schedule dan fungsi F.
 */

/**
 * @brief Rotasi kiri 64 bit.
 *
 * @param[in] x Nilai 64 bit yang diputar.
 * @param[in] n Jumlah posisi (harus 1..63).
 * @return @c x yang sudah diputar kiri sebanyak @c n bit.
 *
 * @warning @c n = 0 atau @c n = 64 menghasilkan
 *          undefined behavior.
 */
inline uint64_t rotl64(uint64_t x, unsigned n){
    return (x << n) | x >> (64 - n);
}


<<<<<<< HEAD
/**
 * @brief Rotasi kiri 32 bit.
 *
 * @param[in] x Nilai 32 bit yang diputar.
 * @param[in] n Jumlah posisi (harus 1..31).
 * @return @c x yang sudah diputar kiri sebanyak @c n bit.
 *
 * @warning @c n = 0 atau @c n = 32 menghasilkan
 *          undefined behavior.
 */
=======
/// Rotasi kiri 32 bit. n harus 1..31 (n = 0 atau 32 -> undefined behavior).
>>>>>>> 9543fe6 (feat:merge makefile:)
inline uint32_t rotl32(uint32_t x, unsigned n){
    return (x << n) | x >> (32 - n);
}

<<<<<<< HEAD
#endif
=======
#endif
>>>>>>> 9543fe6 (feat:merge makefile:)
