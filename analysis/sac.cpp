/**
 * @file sac.cpp
 * @brief Uji Strict Avalanche Criterion (SAC) pada @ref CustomCipher::encrypt_block.
 *
 * Untuk tiap pasangan (bit input i, bit output j), diukur peluang bit output j
 * berubah ketika bit input i dibalik. Cipher yang baik menghasilkan ~50% untuk
 * semua pasangan, bukan hanya rata-ratanya (lebih ketat dari avalanche.cpp).
 *
 * Dilakukan dua kali: 
 * 1. bit plaintext ke bit ciphertext (128 x 128 pasangan), dan
 * 2. bit kunci ke bit ciphertext (128 x 128 pasangan).
 *
 * Karena jumlah percobaan terbatas, tiap sel punya galat acak. Di bawah
 * hipotesis p = 0.5, simpangan bakunya sigma = 50% / sqrt(TRIALS). Sel yang
 * menyimpang lebih dari 4 sigma jarang terjadi (sekitar 6.3e-5 per sel).
 *
 * Jalankan dengan:
 * @code
 * make sac
 * @endcode
 *
 * @see avalanche.cpp untuk uji avalanche rata-rata.
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include "cipher.hpp"

/// Jumlah attempt (pasangan kunci dan plaintext acak).
static const int TRIALS = 2000;
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
 * @brief Ambil satu bit dari buffer.
 * @param[in] p Buffer sumber.
 * @param[in] i Indeks bit (bit 0 = LSB byte pertama).
 * @return 0 atau 1.
 */
static inline int get_bit(const uint8_t *p, size_t i) {
  return (p[i / 8] >> (i % 8)) & 1;
}

/**
 * @brief Cetak ringkasan matriks SAC.
 *
 * Yang diprint: 
 * - rata-rata, 
 * - min, dan 
 * - max persentase seluruh sel
 * - sel dengan simpangan terbesar (dalam satuan sigma), 
 * - serta jumlah sel di luar 4 sigma
 *
 * @param[in] name    Label bagian laporan.
 * @param[in] in_bits Jumlah bit input yang dibalik (jumlah baris matriks).
 * @param[in] counts  counts[i][j] = berapa kali bit output j berubah saat
 *                    bit input i dibalik, dari @c TRIALS attempt.
 */
static void report(const char *name, size_t in_bits,
                   const int counts[][BLOCK_BITS]) {
  const double sigma = 100.0 * 0.5 / std::sqrt((double)TRIALS);  // % , H0: p = 0.5
  double sum = 0, mn = 1e9, mx = -1, worst_dev = 0;
  long cells = 0, beyond4 = 0;
  size_t worst_i = 0, worst_j = 0;

  for (size_t i = 0; i < in_bits; i++) {
    for (size_t j = 0; j < BLOCK_BITS; j++) {
      double pct = 100.0 * counts[i][j] / TRIALS;
      double dev = std::fabs(pct - 50.0) / sigma;
      sum += pct;
      if (pct < mn) mn = pct;
      if (pct > mx) mx = pct;
      if (dev > worst_dev) { worst_dev = dev; worst_i = i; worst_j = j; }
      if (dev > 4.0) beyond4++;
      cells++;
    }
  }

  printf("%s (%zu x %zu pasangan bit)\n", name, in_bits, BLOCK_BITS);
  printf("  rata-rata %.2f%%  min %.2f%%  max %.2f%%\n", sum / cells, mn, mx);
  printf("  simpangan terburuk: %.1f sigma (input bit %zu -> output bit %zu)\n",
         worst_dev, worst_i, worst_j);
  printf("  sel di luar 4 sigma: %ld dari %ld (acak murni ~%.1f)\n",
         beyond4, cells, cells * 6.3e-5);
}

/**
 * @brief SAC untuk bit plaintext dan bit kunci.
 * @return 0 (only report, tidak menentukan pass or fail).
 */
int main() {
  static int pt_counts[BLOCK_BITS][BLOCK_BITS];
  static int key_counts[KEY_BITS][BLOCK_BITS];

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
      for (size_t j = 0; j < BLOCK_BITS; j++)
        pt_counts[i][j] += get_bit(ct, j) ^ get_bit(ct2, j);
    }

    // balik 1 bit kunci
    for (size_t i = 0; i < KEY_BITS; i++) {
      uint8_t key2[KEY_SIZE];
      memcpy(key2, key, KEY_SIZE);
      key2[i / 8] ^= (uint8_t)(1u << (i % 8));
      CustomCipher other(key2);
      other.encrypt_block(pt, ct2);
      for (size_t j = 0; j < BLOCK_BITS; j++)
        key_counts[i][j] += get_bit(ct, j) ^ get_bit(ct2, j);
    }
  }

  printf("SAC (%d percobaan) \n", TRIALS);
  report("Bit plaintext -> bit ciphertext", BLOCK_BITS, pt_counts);
  printf("\n");
  report("Bit kunci -> bit ciphertext", KEY_BITS, key_counts);
  return 0;
}
