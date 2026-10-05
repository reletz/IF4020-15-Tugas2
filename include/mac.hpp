#ifndef MAC_HPP
#define MAC_HPP

#include "cipher.hpp"
#include "util.hpp"
#include <cstddef>
#include <cstdint>

/**
 * @file mac.hpp
 * @brief Implementasi Cipher-based Message Authentication Code (CMAC / OMAC1, RFC 4493)
 *        dan derivasi pemisahan kunci (key separation) menggunakan BlockCipher 128-bit.
 */

/// Ukuran tag MAC dalam byte (128 bit / 16 byte).
constexpr size_t MAC_TAG_SIZE = 16;
static_assert(BLOCK_SIZE == 16, "CMAC di sini mengasumsikan blok 128-bit (Rb = 0x87)");

namespace cmac_detail {
/**
 * @brief Operasi doubling dalam medan GF(2^128) pada blok 16-byte big-endian.
 *
 * Menggeser seluruh blok 128 bit ke kiri sebesar 1 bit secara branch-free.
 * Jika MSB bernilai 1, byte terakhir (byte 15) di-XOR dengan konstanta Rb = 0x87.
 */
void dbl(const uint8_t in[16], uint8_t out[16]);
} // namespace cmac_detail

/**
 * @brief Generator dan verifikator Message Authentication Code berbasis CMAC (RFC 4493).
 */
class Cmac {
public:
    explicit Cmac(const BlockCipher& cipher);
    Cmac(const BlockCipher&&) = delete; // Mencegah dangling reference ke cipher sementara
    ~Cmac();

    Cmac(const Cmac&) = delete;
    Cmac& operator=(const Cmac&) = delete;

    /**
     * @brief Menambahkan chunk data ke dalam perhitungan CMAC secara inkremental.
     * @param[in] data Pointer ke data byte masukan.
     * @param[in] len  Panjang data byte masukan.
     * @throw std::invalid_argument jika data bernilai nullptr saat len > 0.
     */
    void update(const uint8_t* data, size_t len);

    /**
     * @brief Menyelesaikan perhitungan tag CMAC, menuliskan tag ke buffer keluaran,
     *        dan mereset state internal agar objek siap digunakan kembali.
     * @param[out] tag Buffer 16-byte untuk menyimpan tag hasil perhitungan.
     * @throw std::invalid_argument jika tag bernilai nullptr.
     */
    void finalize(uint8_t tag[MAC_TAG_SIZE]);

    /// Menghitung tag CMAC untuk seluruh data dalam satu pemanggilan (one-shot).
    static void compute(const BlockCipher& cipher, const uint8_t* data, size_t len, uint8_t tag[MAC_TAG_SIZE]);

    /// Memverifikasi keabsahan tag CMAC dalam waktu konstan (constant-time).
    static bool verify(const BlockCipher& cipher, const uint8_t* data, size_t len, const uint8_t tag[MAC_TAG_SIZE]);

private:
    const BlockCipher& cipher_;
    uint8_t k1_[BLOCK_SIZE];
    uint8_t k2_[BLOCK_SIZE];
    uint8_t x_[BLOCK_SIZE];
    uint8_t buf_[BLOCK_SIZE];
    size_t buf_len_{0};

    void process_buf();
    void reset_state();
};

/**
 * @brief Struktur pasangan kunci hasil derivasi untuk enkripsi dan autentikasi.
 *
 * Memori enc dan mac otomatis dibersihkan dengan util::secure_zero saat destruksi.
 */
struct DerivedKeys {
    uint8_t enc[KEY_SIZE]{0}; ///< Kunci untuk enkripsi data payload.
    uint8_t mac[KEY_SIZE]{0}; ///< Kunci untuk tag integritas CMAC.

    DerivedKeys() = default;
    ~DerivedKeys() {
        util::secure_zero(enc, sizeof(enc));
        util::secure_zero(mac, sizeof(mac));
    }
    DerivedKeys(const DerivedKeys&) = default;
    DerivedKeys& operator=(const DerivedKeys&) = default;
    DerivedKeys(DerivedKeys&&) noexcept = default;
    DerivedKeys& operator=(DerivedKeys&&) noexcept = default;
};

/**
 * @brief Menghasilkan kunci enkripsi dan kunci MAC terpisah dari master cipher (KDF berbasis PRF).
 *
 * enc = E_master(00 00 ... 00 01)
 * mac = E_master(00 00 ... 00 02)
 */
DerivedKeys derive_keys(const BlockCipher& master_cipher);

#endif // MAC_HPP
