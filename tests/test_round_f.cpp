// tests/test_round_f.cpp
//
// Mencatat sifat fungsi F (round_f) pada implementasi sekarang. Hasilnya dipakai
// sebagai bukti untuk Bab 3 dan Bab 6 laporan:
//   1. F dapat dibalik untuk kunci tertentu (tidak benar-benar lossy),
//   2. tidak ada tabrakan keluaran pada sampel acak,
//   3. F deterministik,
//   4. difusi satu pemanggilan F (dicatat sebagai angka, bukan target 50%).
//
// Tes 1 dan 2 mencatat properti implementasi saat ini, bukan syarat yang harus
// berlaku selamanya: jika kuantisasi F nanti dibuat benar-benar lossy (lihat saran
// pengembangan di laporan), kedua tes ini sengaja akan gagal dan harus dibalik.
#include "bits.hpp"
#include "permutation.hpp"
#include "round.hpp"
#include "sbox.hpp"
#include "test_helpers.hpp"

#include <cstdio>
#include <unordered_set>

namespace {

// PRNG deterministik (xorshift64) agar hasil bisa direproduksi
uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
uint64_t next_rand() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

uint8_t INV_SBOX[256];

uint32_t inv_sbox32(uint32_t a) {
    return ((uint32_t)INV_SBOX[a >> 24] << 24) | ((uint32_t)INV_SBOX[(a >> 16) & 0xFF] << 16) |
           ((uint32_t)INV_SBOX[(a >> 8) & 0xFF] << 8) | INV_SBOX[a & 0xFF];
}

// Invers permutasi bit: bit posisi (i * PERM_MULT) mod 64 dikembalikan ke posisi i.
uint64_t inv_permute(uint64_t y) {
    uint64_t x = 0;
    for (int i = 0; i < 64; i++) x |= ((y >> ((i * PERM_MULT) % 64)) & 1ULL) << i;
    return x;
}

// Invers dua langkah sigma-delta berurutan (a lalu b), akumulator dan umpan balik awal 0.
// Diberikan keluaran (qa, qb) dan kunci (ka, kb), kembalikan masukan (xa, xb).
void inv_two_steps(uint32_t qa, uint32_t qb, uint32_t ka, uint32_t kb, uint32_t &xa, uint32_t &xb) {
    uint32_t acc1 = inv_sbox32(qa) ^ ka;  // acc awal 0, umpan balik awal 0
    xa = acc1;
    uint32_t acc2 = inv_sbox32(qb) ^ kb;
    xb = acc2 - acc1 + qa;  // umpan balik langkah kedua = qa
}

// Invers F untuk kunci k, mengikuti urutan round_f dari belakang.
uint64_t inv_round_f(uint64_t y, uint64_t k) {
    uint32_t k0 = (uint32_t)(k >> 32), k1 = (uint32_t)k;
    uint64_t w = inv_permute(y);
    uint32_t w0 = (uint32_t)(w >> 32), w1 = (uint32_t)w;

    // batalkan pass mundur (w1 dengan k0, lalu w0 dengan k1)
    uint32_t x1, x0;
    inv_two_steps(w1, w0, k0, k1, x1, x0);

    // batalkan rotasi
    x0 = rotl32(x0, 32 - 7);
    x1 = rotl32(x1, 32 - 19);

    // batalkan pass maju (w0 dengan k0, lalu w1 dengan k1)
    uint32_t r0, r1;
    inv_two_steps(x0, x1, k0, k1, r0, r1);
    return ((uint64_t)r0 << 32) | r1;
}

int popcount64(uint64_t v) {
    int c = 0;
    while (v) { c += (int)(v & 1); v >>= 1; }
    return c;
}

}  // namespace

void test_f_invertible() {
    const int N = 100000;
    int failures = 0;
    for (int t = 0; t < N; t++) {
        uint64_t r = next_rand(), k = next_rand();
        if (inv_round_f(round_f(r, k), k) != r) failures++;
    }
    printf("F dapat dibalik untuk %d dari %d pasangan (R, K) acak\n", N - failures, N);
    CHECK(failures == 0, "F dapat dibalik untuk kunci tertentu pada sampel acak");
}

// Catatan: pada ruang keluaran 2^64, 10^5 sampel hampir tidak bisa mendeteksi lossiness
// ringan (peluang tabrakan ~ 1e-9 walau F kehilangan beberapa bit). Tes ini hanya
// pelengkap; bukti utama ketidak-lossy-an F adalah tes invertibilitas di atas.
void test_f_no_collisions() {
    const int N = 100000;
    const uint64_t k = next_rand();
    std::unordered_set<uint64_t> seen;
    seen.reserve(N * 2);
    int collisions = 0;
    for (int t = 0; t < N; t++) {
        uint64_t y = round_f(next_rand(), k);
        if (!seen.insert(y).second) collisions++;
    }
    printf("Tabrakan keluaran F pada %d masukan acak (kunci tetap): %d\n", N, collisions);
    CHECK(collisions == 0, "tidak ada tabrakan keluaran F pada sampel acak");
}

void test_f_deterministic() {
    bool same = true;
    for (int t = 0; t < 1000; t++) {
        uint64_t r = next_rand(), k = next_rand();
        if (round_f(r, k) != round_f(r, k)) same = false;
    }
    CHECK(same, "F deterministik: masukan sama menghasilkan keluaran sama");
}

void test_f_diffusion() {
    // Balik 1 bit R pada tiap posisi, catat rata-rata bit keluaran yang berubah.
    // Ini difusi SATU pemanggilan F, bukan seluruh cipher: carry penjumlahan hanya
    // merambat ke bit yang lebih tinggi, sehingga angkanya wajar di bawah 32 dari 64.
    // Difusi penuh baru tercapai lewat 16 putaran (lihat avalanche per round).
    const int TRIALS = 2000;
    double sum[64] = {0};
    for (int t = 0; t < TRIALS; t++) {
        uint64_t r = next_rand(), k = next_rand();
        uint64_t base = round_f(r, k);
        for (int b = 0; b < 64; b++)
            sum[b] += popcount64(base ^ round_f(r ^ (1ULL << b), k));
    }
    double total = 0, mn = 1e9, mx = -1;
    for (int b = 0; b < 64; b++) {
        double m = sum[b] / TRIALS;
        total += m;
        if (m < mn) mn = m;
        if (m > mx) mx = m;
    }
    double mean = total / 64;
    printf("Bit keluaran F yang berubah untuk 1 bit masukan: rata-rata %.2f, "
           "per posisi min %.2f max %.2f (dari 64)\n", mean, mn, mx);
    // Batas longgar: hanya memastikan F tidak degenerate (tidak konstan, tidak identitas).
    CHECK(mn > 1.0, "setiap bit masukan memengaruhi keluaran F");
    CHECK(mean > 8.0 && mean < 56.0, "difusi satu pemanggilan F tidak degenerate");
}

int main() {
    for (int i = 0; i < 256; i++) INV_SBOX[SBOX[i]] = (uint8_t)i;

    std::printf("--- Unit Test round_f ---\n");
    test_f_invertible();
    test_f_no_collisions();
    test_f_deterministic();
    test_f_diffusion();
    return test::report();
}
