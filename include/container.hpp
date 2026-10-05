#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include "cipher.hpp"
#include "mac.hpp"
#include "util.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

/**
 * @file container.hpp
 * @brief Format kontainer terenkripsi v1 dan pipeline Encrypt-then-MAC (SecureEnvelope).
 */

/// Identifikasi mode operasi block cipher.
enum class Mode : uint8_t {
    ECB = 1,
    CBC = 2,
    CFB = 3,
    OFB = 4,
    CTR = 5
};

/// Panjang header kontainer (Magic 4B + Version 1B + Mode 1B + IV 16B).
constexpr size_t CONTAINER_HEADER_SIZE = 22;
/// Ukuran minimum kontainer v1 (Header 22B + MAC Tag 16B).
constexpr size_t CONTAINER_MIN_SIZE = CONTAINER_HEADER_SIZE + MAC_TAG_SIZE;

/**
 * @brief Parsing string representasi mode menjadi enum Mode (case-insensitive).
 * @param[in] str Nama mode ("ecb".."ctr").
 * @return Mode Nilai enum Mode.
 * @throw std::invalid_argument jika nama mode tidak dikenali.
 */
Mode parse_mode(const std::string& str);

/**
 * @brief Mengembalikan nama mode dalam bentuk string huruf kecil ("ecb".."ctr").
 * @param[in] mode Nilai enum Mode.
 * @return std::string Nama mode.
 */
std::string mode_name(Mode mode);

/// Exception saat verifikasi integritas tag MAC gagal.
class AuthenticationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Exception saat struktur format kontainer tidak valid.
class FormatError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief Pengelola enkripsi dan autentikasi kontainer berkas (Encrypt-then-MAC).
 */
class SecureEnvelope {
public:
    /**
     * @brief Inisialisasi envelope dan derivasi kunci enkripsi serta kunci MAC.
     * @param[in] master_key Master key berukuran 16 byte (128 bit).
     */
    explicit SecureEnvelope(const uint8_t master_key[KEY_SIZE]);

    /**
     * @brief Mengenkripsi plaintext ke format kontainer v1 terotentikasi.
     * @param[in] plaintext Data yang akan dienkripsi.
     * @param[in] mode      Mode operasi yang digunakan.
     * @return util::Bytes  Blob kontainer lengkap.
     */
    util::Bytes seal(const util::Bytes& plaintext, Mode mode) const;

    /**
     * @brief Memverifikasi integritas kontainer dan mendekripsi isinya.
     * @param[in] blob Kontainer biner lengkap.
     * @return util::Bytes Plaintext hasil dekripsi.
     * @throw FormatError jika struktur/panjang header kontainer cacat.
     * @throw AuthenticationError jika verifikasi tag CMAC gagal.
     */
    util::Bytes open(const util::Bytes& blob) const;

private:
    explicit SecureEnvelope(DerivedKeys keys);

    CustomCipher enc_cipher_;
    CustomCipher mac_cipher_;
};

#endif // CONTAINER_HPP
