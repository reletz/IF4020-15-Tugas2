#ifndef SBOX_HPP
#define SBOX_HPP
#include <cstdint>

/**
 * @file sbox.hpp
 * @brief Substitusi byte berbasis S-box (table lookup).
 *
 * S-box bekerja per byte: tiap byte input diganti dengan nilai dari tabel
 * 256 entri. Dua fungsi di bawah memakai tabel yang sama, hanya berbeda lebar
 * input.
 *
 * @brief S-box 8-bit: substitusi bijektif tanpa fixed point.
 *
 * Dibangkitkan oleh tools/gen_sbox.cpp dengan PRNG xorshift64
 * (seed = digit pi) dan Fisher-Yates shuffle, dipilih dari 10000
 * kandidat berdasarkan nonlinearity tertinggi lalu differential
 * uniformity terendah.
 *
 * Jalankan `make sbox` untuk regenerasi deterministik.
 */
/// Tabel S-box 256 entri, bijektif, tidak ada fixed point.
extern const uint8_t SBOX[256];
/**
 * @brief Substitusi satu byte.
 * @param x  Input byte.
 * @return   SBOX[x].
 */
uint8_t sbox(uint8_t x);
/**
 * @brief Terapkan S-box secara independen ke tiap byte sebuah 32-bit word.
 *
 * Dipakai di fungsi F (round function): q = sbox32(acc ^ K_i[j]).
 * Setara dengan menjalankan sbox() pada byte [3], [2], [1], [0] acc.
 *
 * @param acc  32-bit accumulator dari loop sigma-delta.
 * @return     Word hasil substitusi tiap byte.
 */
uint32_t sbox32(uint32_t acc);
#endif