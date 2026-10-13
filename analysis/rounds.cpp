/**
 * @file rounds.cpp
 * @brief Avalanche effect untuk 1 sampai @ref ROUNDS round Feistel.
 *
 * Memakai @ref key_schedule dan @ref round_f yang sama dengan @ref CustomCipher.
 * Versi @ref ROUNDS round dicek identik dengan @ref CustomCipher::encrypt_block.
 *
 * @code
 * make rounds
 * make rounds ROUNDS_ARGS="--csv out.csv"
 * @endcode
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cipher.hpp"
#include "key_schedule.hpp"
#include "round.hpp"

/// Jumlah percobaan (pasangan kunci dan plaintext acak) per jumlah round.
static const int TRIALS = 500;
/// Ukuran blok dalam bit.
constexpr size_t BLOCK_BITS = BLOCK_SIZE * 8;
/// Ukuran kunci dalam bit.
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
 * @brief Baca 8 byte big-endian menjadi 64 bit (sama dengan cipher.cpp).
 * @param[in] p Buffer 8 byte.
 * @return Nilai 64 bit.
 */
static uint64_t load64(const uint8_t *p) {
  uint64_t v = 0;
  for (size_t i = 0; i < 8; i++) v = (v << 8) | p[i];
  return v;
}

/**
 * @brief Tulis 64 bit ke 8 byte big-endian (sama dengan cipher.cpp).
 * @param[out] p Buffer 8 byte.
 * @param[in]  v Nilai 64 bit.
 */
static void store64(uint8_t *p, uint64_t v) {
  for (size_t i = 8; i-- > 0; v >>= 8) p[i] = (uint8_t)v;
}

/**
 * @brief Bangkitkan round key dari master key 16 byte.
 * @param[in]  key Master key.
 * @param[out] rk  Round key K_0 ... K_15.
 */
static void make_round_keys(const uint8_t key[KEY_SIZE], uint64_t rk[ROUNDS]) {
  key_schedule(load64(key), load64(key + 8), rk);
}

/**
 * @brief Enkripsi satu blok dengan hanya r round pertama (termasuk swap terakhir).
 * @param[in]  rk  Round key.
 * @param[in]  r   Jumlah round (1..ROUNDS).
 * @param[in]  in  Plaintext 16 byte.
 * @param[out] out Ciphertext 16 byte.
 */
static void encrypt_rounds(const uint64_t rk[ROUNDS], int r, const uint8_t *in, uint8_t *out) {
  uint64_t L = load64(in), R = load64(in + 8);
  for (int i = 0; i < r; i++) {
    uint64_t next = L ^ round_f(R, rk[i]);
    L = R;
    R = next;
  }
  store64(out, R);
  store64(out + 8, L);
}

/**
 * @brief Hitung jumlah bit yang berbeda antara dua blok.
 * @param[in] a Blok pertama.
 * @param[in] b Blok kedua.
 * @return Jumlah bit yang berbeda (0..128).
 */
static int diff_bits(const uint8_t *a, const uint8_t *b) {
  int bits = 0;
  for (size_t i = 0; i < BLOCK_SIZE; i++) bits += __builtin_popcount((unsigned)(a[i] ^ b[i]));
  return bits;
}

/**
 * @brief Pastikan versi r = ROUNDS identik dengan CustomCipher::encrypt_block.
 * @return true jika identik untuk 100 blok acak.
 */
static bool self_check() {
  for (int t = 0; t < 100; t++) {
    uint8_t key[KEY_SIZE], pt[BLOCK_SIZE], a[BLOCK_SIZE], b[BLOCK_SIZE];
    uint64_t rk[ROUNDS];
    fill_random(key, KEY_SIZE);
    fill_random(pt, BLOCK_SIZE);
    make_round_keys(key, rk);
    encrypt_rounds(rk, ROUNDS, pt, a);
    CustomCipher(key).encrypt_block(pt, b);
    if (std::memcmp(a, b, BLOCK_SIZE) != 0) return false;
  }
  return true;
}

/**
 * @brief Avalanche plaintext dan kunci untuk r = 1..ROUNDS.
 * @param[in] argc Jumlah argumen.
 * @param[in] argv Opsional @c --csv PATH untuk menyimpan hasil.
 * @return 0 jika selesai, 1 jika self-check gagal atau argumen salah.
 */
int main(int argc, char **argv) {
  const char *csv_path = nullptr;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--csv" && i + 1 < argc) {
      csv_path = argv[++i];
    } else {
      std::fprintf(stderr, "Pakai: rounds [--csv PATH]\n");
      return 1;
    }
  }

  if (!self_check()) {
    std::fprintf(stderr, "Self-check gagal: encrypt_rounds(16) != CustomCipher::encrypt_block\n");
    return 1;
  }

  FILE *csv = nullptr;
  if (csv_path) {
    csv = std::fopen(csv_path, "w");
    if (!csv) {
      std::fprintf(stderr, "Gagal menulis %s\n", csv_path);
      return 1;
    }
    std::fprintf(csv, "round,pt_rata2_pct,pt_min_pct,pt_max_pct,kunci_rata2_pct,kunci_min_pct,kunci_max_pct\n");
  }

  printf("Avalanche per jumlah round (%d percobaan x 128 bit, self-check r=%d == CustomCipher: OK)\n\n",
         TRIALS, ROUNDS);
  printf("%5s | %-30s | %-30s\n", "round", "1 bit plaintext rata2 (min-max)", "1 bit kunci rata2 (min-max)");
  printf("------+--------------------------------+-------------------------------\n");

  for (int r = 1; r <= ROUNDS; r++) {
    rng_state = 0x123456789ABCDEF0ULL; // data uji sama untuk setiap r
    double pt_sum = 0, key_sum = 0;
    int pt_min = 999, pt_max = -1, key_min = 999, key_max = -1;
    long n = 0;

    for (int t = 0; t < TRIALS; t++) {
      uint8_t key[KEY_SIZE], pt[BLOCK_SIZE], ct[BLOCK_SIZE], ct2[BLOCK_SIZE];
      uint64_t rk[ROUNDS];
      fill_random(key, KEY_SIZE);
      fill_random(pt, BLOCK_SIZE);
      make_round_keys(key, rk);
      encrypt_rounds(rk, r, pt, ct);

      for (size_t i = 0; i < BLOCK_BITS; i++) {
        uint8_t pt2[BLOCK_SIZE];
        std::memcpy(pt2, pt, BLOCK_SIZE);
        pt2[i / 8] ^= (uint8_t)(1u << (i % 8));
        encrypt_rounds(rk, r, pt2, ct2);
        int d = diff_bits(ct, ct2);
        pt_sum += d;
        if (d < pt_min) pt_min = d;
        if (d > pt_max) pt_max = d;
      }
      for (size_t i = 0; i < KEY_BITS; i++) {
        uint8_t key2[KEY_SIZE];
        uint64_t rk2[ROUNDS];
        std::memcpy(key2, key, KEY_SIZE);
        key2[i / 8] ^= (uint8_t)(1u << (i % 8));
        make_round_keys(key2, rk2);
        encrypt_rounds(rk2, r, pt, ct2);
        int d = diff_bits(ct, ct2);
        key_sum += d;
        if (d < key_min) key_min = d;
        if (d > key_max) key_max = d;
      }
      n += BLOCK_BITS;
    }

    double pt_mean = 100.0 * pt_sum / (n * (double)BLOCK_BITS);
    double key_mean = 100.0 * key_sum / (n * (double)BLOCK_BITS);
    double k = 100.0 / BLOCK_BITS;
    printf("%5d | %6.2f%% (%5.2f%% - %5.2f%%)      | %6.2f%% (%5.2f%% - %5.2f%%)\n", r, pt_mean, pt_min * k,
           pt_max * k, key_mean, key_min * k, key_max * k);
    if (csv)
      std::fprintf(csv, "%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", r, pt_mean, pt_min * k, pt_max * k, key_mean,
                   key_min * k, key_max * k);
  }
  if (csv) std::fclose(csv);
  return 0;
}
