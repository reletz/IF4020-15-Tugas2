#ifndef SBOX_HPP
#define SBOX_HPP
#include <cstdint>

/**
 * @file sbox.hpp
 * @brief S-box 8-bit: substitusi byte bijektif tanpa fixed point.
 *
 * Tabel dibangkitkan oleh tools/gen_sbox.cpp dengan PRNG xorshift64
 * (seed = digit pi) dan Fisher-Yates shuffle, dipilih dari 10000
 * kandidat berdasarkan nonlinearity tertinggi lalu differential
 * uniformity terendah. Jalankan `make sbox` untuk regenerasi deterministik.
 *
 * Semua fungsi substitusi memakai tabel @ref SBOX yang sama, hanya berbeda
 * lebar input.
 */

/// Tabel S-box 256 entri, bijektif, tidak ada fixed point.
extern const uint8_t SBOX[256];

/**
 * @brief Substitusi satu byte.
 *
 * @param[in] x Byte input.
 * @return SBOX[x].
 */
uint8_t sbox(uint8_t x);

/**
 * @brief Substitusi tiap byte pada nilai 32 bit (1 word).
 *
 * Tiap dari 4 byte diganti dengan nilai @ref SBOX secara independen.
 * Dipakai di fungsi F (round function).
 *
 * @param[in] a Nilai 32 bit.
 * @return Nilai dengan tiap byte sudah disubstitusi.
 */
uint32_t sbox32(uint32_t a);

/**
 * @brief Substitusi tiap byte pada nilai 64 bit.
 *
 * Tiap dari 8 byte diganti dengan nilai @ref SBOX secara independen.
 * Dipakai di key schedule.
 *
 * @param[in] a Nilai 64 bit.
 * @return Nilai dengan tiap byte sudah disubstitusi.
 */
uint64_t sbox64(uint64_t a);

#endif
