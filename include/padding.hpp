#ifndef PADDING_HPP
#define PADDING_HPP

#include "cipher.hpp"
#include "util.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

/**
 * @file padding.hpp
 * @brief Implementasi skema padding dan unpadding PKCS#7 untuk block cipher.
 */

/**
 * @brief Exception yang dilempar saat verifikasi padding PKCS#7 gagal.
 */
class PaddingError : public std::runtime_error {
public:
    explicit PaddingError(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Menambahkan padding PKCS#7 pada data masukan.
 *
 * Selalu menambahkan 1 sampai @ref BLOCK_SIZE (16) byte ke dalam data masukan
 * sehingga panjang keluaran selalu merupakan kelipatan dari ukuran blok.
 * Jika masukan sudah merupakan kelipatan ukuran blok (termasuk 0 byte),
 * satu blok penuh berisi byte 0x10 (16) akan ditambahkan.
 *
 * @param[in] in Data masukan yang akan dipad.
 * @return util::Bytes Data hasil padding dengan panjang kelipatan @ref BLOCK_SIZE.
 *
 * @code
 * util::Bytes pt = {0x01, 0x02};
 * util::Bytes padded = pkcs7_pad(pt); // ukuran 16, 14 byte terakhir bernilai 0x0E
 * @endcode
 */
util::Bytes pkcs7_pad(const util::Bytes& in);

/**
 * @brief Memvalidasi dan menghapus padding PKCS#7 dari data masukan.
 *
 * Memeriksa apakah data masukan tidak kosong, panjangnya kelipatan @ref BLOCK_SIZE,
 * nilai byte padding terakhir berada di rentang 1..@ref BLOCK_SIZE, serta
 * seluruh byte padding di akhir bernilai seragam.
 *
 * @param[in] in Data berpadding yang akan diunpad.
 * @return util::Bytes Data asli setelah byte padding dihapus.
 * @throw PaddingError Jika format atau isi padding tidak valid.
 *
 * @code
 * util::Bytes original = pkcs7_unpad(padded);
 * @endcode
 */
util::Bytes pkcs7_unpad(const util::Bytes& in);

#endif
