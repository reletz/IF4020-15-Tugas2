/**
 * @file avalanche.cpp
 * @brief Uji avalanche effect pada @ref CustomCipher::encrypt_block.
 *
 * Satu bit plaintext (atau satu bit kunci) dibalik, lalu dihitung persentase
 * bit ciphertext yang berubah. Cipher yang baik menghasilkan nilai mendekati
 * 50% (ideal).
 *
 * Tiap percobaan memakai kunci dan plaintext acak, lalu membalik seluruh
 * 128 bit plaintext dan 128 bit kunci satu per satu.
 *
 * Jalankan dengan:
 * @code
 * make avalanche
 * @endcode
 *
 * @see sac.cpp untuk uji per pasangan bit input dan output.
 */
#include <cstdio>
#include <cstring>
#include "cipher.hpp"

/// Jumlah attemp (pasangan kunci dan plaintext acak).
static const int TRIALS = 1000;
/// Ukuran block dalam bit.
constexpr size_t BLOCK_BITS = BLOCK_SIZE * 8;
/// Ukuran key dalam bit.
constexpr size_t KEY_BITS = KEY_SIZE * 8;

/// State PRNG. Seed tetap supaya hasil uji bisa direproduksi.
static uint64_t rng_state = 0x123456789ABCDEF0ULL;

/**
 * @brief PRNG deterministik (xorshift64).
 * @return Nilai acak 64 bit berikutnya.
 * @note Bukan PRNG kriptografis, hanya untuk generate test data.
 */
static uint64_t next_rand() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}

/**
 * @brief Isi buffer dengan byte acak dari @c next_rand().
 * @param[out] p Buffer tujuan.
 * @param[in]  n Jumlah byte yang diisi.
 */
static void fill_random(uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) p[i] = (uint8_t)next_rand();
}

/**
 * @brief Hitung jumlah bit yang berbeda antara dua buffer (jarak Hamming).
 * @param[in] a Buffer pertama.
 * @param[in] b Buffer kedua.
 * @param[in] n Panjang kedua buffer dalam byte.
 * @return Jumlah bit yang berbeda, antara 0 dan 8 * n.
 */
static int popcount_diff(const uint8_t *a, const uint8_t *b, size_t n) {
  int bits = 0;
  for (size_t i = 0; i < n; i++) {
    uint8_t d = a[i] ^ b[i];
    while (d) { bits += d & 1; d >>= 1; }
  }
  return bits;
}

/**
 * @brief Statistik persentase bit yang berubah (rata-rata, min, max).
 */
struct Stat {
  double sum = 0;   ///< Jumlah seluruh persentase yang dicatat.
  double min = 1e9; ///< Persentase terkecil.
  double max = -1;  ///< Persentase terbesar.
  long count = 0;   ///< Banyaknya sampel.

  /**
   * @brief Catat satu sampel.
   * @param[in] changed Jumlah bit ciphertext yang berubah (0 sampai @ref BLOCK_BITS).
   */
  void add(int changed) {
    double pct = 100.0 * changed / BLOCK_BITS;
    sum += pct;
    if (pct < min) min = pct;
    if (pct > max) max = pct;
    count++;
  }

  /**
   * @brief Print ringkasan statistik ke stdout.
   * @param[in] name Label baris keluaran.
   */
  void print(const char *name) const {
    printf("%-22s rata-rata %.2f%%  min %.2f%%  max %.2f%%\n",
           name, sum / count, min, max);
  }
};

/**
 * @brief Tes avalanche untuk bit plaintext dan bit kunci.
 * @return 0 (only report, tidak menentukan pass or fail).
 */
int main() {
  Stat pt_stat, key_stat;

  for (int t = 0; t < TRIALS; t++) {
    uint8_t key[KEY_SIZE], pt[BLOCK_SIZE];
    uint8_t ct[BLOCK_SIZE], ct2[BLOCK_SIZE];
    fill_random(key, KEY_SIZE);
    fill_random(pt, BLOCK_SIZE);

    CustomCipher base(key);
    base.encrypt_block(pt, ct);

    // balik 1 bit plaintext
    for (size_t i = 0; i < BLOCK_BITS; i++) {
      uint8_t pt2[BLOCK_SIZE];
      memcpy(pt2, pt, BLOCK_SIZE);
      pt2[i / 8] ^= (uint8_t)(1u << (i % 8));
      base.encrypt_block(pt2, ct2);
      pt_stat.add(popcount_diff(ct, ct2, BLOCK_SIZE));
    }

    // balik 1 bit kunci
    for (size_t i = 0; i < KEY_BITS; i++) {
      uint8_t key2[KEY_SIZE];
      memcpy(key2, key, KEY_SIZE);
      key2[i / 8] ^= (uint8_t)(1u << (i % 8));
      CustomCipher other(key2);
      other.encrypt_block(pt, ct2);
      key_stat.add(popcount_diff(ct, ct2, BLOCK_SIZE));
    }
  }

  printf("Avalanche (%d attempt, block %zu bit, key %zu bit)\n",
         TRIALS, BLOCK_BITS, KEY_BITS);
  pt_stat.print("1 bit plaintext");
  key_stat.print("1 bit kunci");
  return 0;
}
