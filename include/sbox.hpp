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
 */

/**
 * @brief Substitusi tiap byte pada nilai 64 bit.
 *
 * Tiap dari 8 byte diganti dengan nilai tabel S-box
 * secara independen.
 *
 * @param[in] a Nilai 64 bit.
 * @return Nilai dengan tiap byte sudah disubstitusi.
 */
uint64_t sbox_bytes(uint64_t a);

/**
 * @brief Substitusi tiap byte pada nilai 32 bit (1 word).
 *
 * Tiap dari 4 byte diganti dengan nilai tabel S-box 
 * yang sama dengan @ref sbox_bytes.
 *
 * @param[in] a Nilai 32 bit.
 * @return Nilai dengan tiap byte sudah disubstitusi.
 */
uint32_t sbox_bytes32(uint32_t a);

#endif