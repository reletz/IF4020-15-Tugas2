#ifndef MODES_HPP
#define MODES_HPP

#include "cipher.hpp"
#include "util.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

/**
 * @file modes.hpp
 * @brief Implementasi mode operasi block cipher (ECB, CBC, CFB, OFB, CTR).
 */

namespace modes {

/// Ambang batas minimum blok untuk mengaktifkan paralelisasi OpenMP (1024 blok = 16 KiB).
constexpr size_t PARALLEL_THRESHOLD_BLOCKS = 1024;

/**
 * @brief Menghitung nilai counter 128-bit big-endian pada indeks blok ke-i langsung: (IV + i) mod 2^128.
 *
 * Fungsi ini mendukung carry 128-bit penuh dan wrap-around dari 0xFF..FF ke 0x00..00.
 * Digunakan baik pada eksekusi serial maupun paralelisasi multi-thread OpenMP
 * agar setiap thread dapat menghitung counter blok tanpa iterasi dari awal.
 *
 * @param[in]  iv  Nilai IV / counter awal (16 byte).
 * @param[in]  i   Indeks blok (offset penambahan counter 64-bit).
 * @param[out] out Buffer keluaran hasil (16 byte).
 * @throw std::invalid_argument jika iv atau out bernilai nullptr.
 */
void ctr_at(const uint8_t iv[BLOCK_SIZE], uint64_t i, uint8_t out[BLOCK_SIZE]);

/**
 * @brief Enkripsi data menggunakan mode Electronic Codebook (ECB).
 *
 * Setiap blok 16 byte dienkripsi secara independen: C_i = E(P_i).
 * Mendukung akselerasi OpenMP multi-thread jika ukuran data >= @ref PARALLEL_THRESHOLD_BLOCKS.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Parameter IV (diabaikan pada mode ECB, boleh nullptr).
 * @param[in] in     Data plaintext yang sudah dipad (kelipatan @ref BLOCK_SIZE).
 * @return util::Bytes Data ciphertext hasil enkripsi.
 * @throw std::invalid_argument jika panjang masukan bukan kelipatan @ref BLOCK_SIZE.
 */
util::Bytes ecb_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Dekripsi data menggunakan mode Electronic Codebook (ECB).
 *
 * Setiap blok 16 byte didekripsi secara independen: P_i = D(C_i).
 * Mendukung akselerasi OpenMP multi-thread jika ukuran data >= @ref PARALLEL_THRESHOLD_BLOCKS.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Parameter IV (diabaikan pada mode ECB, boleh nullptr).
 * @param[in] in     Data ciphertext (kelipatan @ref BLOCK_SIZE).
 * @return util::Bytes Data plaintext hasil dekripsi (masih berpadding).
 * @throw std::invalid_argument jika panjang masukan bukan kelipatan @ref BLOCK_SIZE.
 */
util::Bytes ecb_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Enkripsi data menggunakan mode Cipher Block Chaining (CBC).
 *
 * Formula: C_0 = E(P_0 ^ IV), C_i = E(P_i ^ C_{i-1}) untuk i >= 1.
 * Bersifat serial murni karena blok C_i bergantung pada output blok sebelumnya.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Inisialisasi vektor (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data plaintext yang sudah dipad (kelipatan @ref BLOCK_SIZE).
 * @return util::Bytes Data ciphertext hasil enkripsi.
 * @throw std::invalid_argument jika in bukan kelipatan @ref BLOCK_SIZE atau iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes cbc_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Dekripsi data menggunakan mode Cipher Block Chaining (CBC).
 *
 * Formula: P_0 = D(C_0) ^ IV, P_i = D(C_i) ^ C_{i-1} untuk i >= 1.
 * Mendukung akselerasi OpenMP multi-thread karena seluruh blok ciphertext sudah tersedia.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Inisialisasi vektor (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data ciphertext (kelipatan @ref BLOCK_SIZE).
 * @return util::Bytes Data plaintext hasil dekripsi (masih berpadding).
 * @throw std::invalid_argument jika in bukan kelipatan @ref BLOCK_SIZE atau iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes cbc_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Enkripsi data menggunakan mode Cipher Feedback (CFB-128 full block).
 *
 * Formula: C_i = P_i ^ E(FB_{i-1}), FB_0 = IV, FB_i = C_i.
 * Bersifat serial murni saat enkripsi.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Inisialisasi vektor (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data plaintext sembarang.
 * @return util::Bytes Data ciphertext (panjang sama persis dengan plaintext).
 * @throw std::invalid_argument jika iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes cfb_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Dekripsi data menggunakan mode Cipher Feedback (CFB-128 full block).
 *
 * Formula: P_i = C_i ^ E(FB_{i-1}), FB_0 = IV, FB_i = C_i.
 * Mendukung akselerasi OpenMP multi-thread karena ciphertext sebelumnya sudah diketahui.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Inisialisasi vektor (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data ciphertext sembarang.
 * @return util::Bytes Data plaintext (panjang sama persis dengan ciphertext).
 * @throw std::invalid_argument jika iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes cfb_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

/**
 * @brief Enkripsi/dekripsi data menggunakan mode Output Feedback (OFB).
 *
 * Formula: O_0 = IV, O_i = E(O_{i-1}), C_i = P_i ^ O_i (dan sebaliknya).
 * Bersifat serial murni karena setiap keystream bergantung pada keystream sebelumnya.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Inisialisasi vektor (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data masukan sembarang.
 * @return util::Bytes Data keluaran (panjang sama persis dengan masukan).
 * @throw std::invalid_argument jika iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes ofb_crypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

inline util::Bytes ofb_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    return ofb_crypt(cipher, iv, in);
}

inline util::Bytes ofb_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    return ofb_crypt(cipher, iv, in);
}

/**
 * @brief Enkripsi/dekripsi data menggunakan mode Counter (CTR).
 *
 * Formula: K_i = E(IV + i), C_i = P_i ^ K_i (dan sebaliknya).
 * Mendukung akselerasi OpenMP multi-thread penuh jika ukuran data >= @ref PARALLEL_THRESHOLD_BLOCKS.
 *
 * @param[in] cipher Objek block cipher yang digunakan.
 * @param[in] iv     Nilai counter awal 128-bit (IV), panjang @ref BLOCK_SIZE byte.
 * @param[in] in     Data masukan sembarang.
 * @return util::Bytes Data keluaran (panjang sama persis dengan masukan).
 * @throw std::invalid_argument jika iv bernilai nullptr saat in tidak kosong.
 */
util::Bytes ctr_crypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

inline util::Bytes ctr_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    return ctr_crypt(cipher, iv, in);
}

inline util::Bytes ctr_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    return ctr_crypt(cipher, iv, in);
}

} // namespace modes

#endif
