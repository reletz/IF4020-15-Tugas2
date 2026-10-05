#ifndef CIPHER_HPP
#define CIPHER_HPP

#include <cstddef>
#include <cstdint>

/**
 * @file cipher.hpp
 * @brief Interface block cipher 128-bit dan implementasinya.
 */

/// Ukuran satu blok dalam byte (128 bit, 16x8 bit).
constexpr size_t BLOCK_SIZE = 16;
/// Ukuran master key dalam byte (128 bit, 16x8 bit).
constexpr size_t KEY_SIZE = 16;
/// Jumlah round Feistel.
constexpr int ROUNDS = 16;

/**
 * @brief Interface block cipher 128-bit.
 *
 * Mode operasi (ECB, CBC, CFB, OFB, CTR) hanya bergantung pada kelas ini,
 * sehingga cipher lain bisa dipasang tanpa mengubah kode mode.
 *
 * @c in dan @c out harus menunjuk ke @ref BLOCK_SIZE byte. 
 * Keduanya boleh sama persis (in-place), tetapi tidak boleh overlap sebagian.
 */
class BlockCipher {
public:
    virtual ~BlockCipher() = default;

    /**
     * @brief Enkripsi satu blok.
     * @param[in]  in  Plaintext, @ref BLOCK_SIZE byte.
     * @param[out] out Ciphertext, @ref BLOCK_SIZE byte.
     */
    virtual void encrypt_block(const uint8_t *in, uint8_t *out) const = 0;

    /**
     * @brief Dekripsi satu blok.
     * @param[in]  in  Ciphertext, @ref BLOCK_SIZE byte.
     * @param[out] out Plaintext, @ref BLOCK_SIZE byte.
     */
    virtual void decrypt_block(const uint8_t *in, uint8_t *out) const = 0;
};

/**
 * @brief Block cipher Feistel 16 round dengan fungsi F berbasis sigma-delta.
 *
 * Blok 128 bit dibelah menjadi L dan R (masing-masing 64 bit). Tiap round:
 * @code
 * L' = R
 * R' = L xor F(R, K[i])
 * @endcode
 * Dekripsi memakai alur yang sama dengan urutan round key dibalik, 
 * sehingga F tidak perlu punya invers.
 *
 * Round key dibangkitkan sekali di constructor.
 *
 * @note Objek bersifat immutable setelah dibuat, jadi aman dipakai bersamaan
 *       dari beberapa thread untuk read (encrypt/decrypt).
 *
 * Contoh penggunaan:
 * @code
 * uint8_t key[KEY_SIZE] = { ... };
 * uint8_t pt[BLOCK_SIZE] = { ... };
 * uint8_t ct[BLOCK_SIZE], back[BLOCK_SIZE];
 *
 * CustomCipher cipher(key);
 * cipher.encrypt_block(pt, ct);
 * cipher.decrypt_block(ct, back);   // back == pt
 * @endcode
 */
class CustomCipher : public BlockCipher {
public:
    /**
     * @brief Build cipher dan run key schedule.
     * @param[in] master_key Master key, @ref KEY_SIZE byte. Derive ke round
     *                       key saat constructor dijalankan; pointer tidak disimpan.
     */
    explicit CustomCipher(const uint8_t master_key[KEY_SIZE]);

    void encrypt_block(const uint8_t *in, uint8_t *out) const override;
    void decrypt_block(const uint8_t *in, uint8_t *out) const override;

private:
    uint64_t rk_[ROUNDS];  ///< Round key K[0] ... K[15], masing-masing 64 bit.
};

#endif
