/**
 * @file mode_analysis.cpp
 * @brief Analisis kelima mode operasi: bukti padding, avalanche, entropi, chi-square, dan histogram.
 *
 * Yang diuji adalah ciphertext mentah dari fungsi @c modes:: (tanpa header dan tag).
 * ECB dan CBC dipad PKCS#7. Data uji, kunci, dan IV memakai seed tetap.
 *
 * @code
 * make mode-analysis
 * make mode-analysis ANALYSIS_ARGS="--trials 100 --runs 3 --size 256"
 * @endcode
 *
 * Keluaran CSV (default ke test-results/data): @c avalanche_modes.csv,
 * @c entropy_modes.csv, dan @c hist_<input>.csv.
 */
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "cipher.hpp"
#include "modes.hpp"
#include "padding.hpp"

namespace fs = std::filesystem;

/// Signature fungsi mode, sama dengan yang dipakai container.
using ModeFn = util::Bytes (*)(const BlockCipher &, const uint8_t *, const util::Bytes &);

/// Deskripsi satu mode yang dianalisis.
struct ModeInfo {
  const char *name; ///< Nama mode.
  bool pad;         ///< true jika plaintext dipad PKCS#7 sebelum enkripsi.
  bool uses_iv;     ///< true jika mode memakai IV / counter awal.
  ModeFn enc;       ///< Fungsi enkripsi mode.
};

/// Kelima mode, dengan aturan padding yang sama dengan @ref SecureEnvelope.
static const ModeInfo MODES[] = {
    {"ECB", true, false, modes::ecb_encrypt},
    {"CBC", true, true, modes::cbc_encrypt},
    {"CFB", false, true, modes::cfb_encrypt},
    {"OFB", false, true, modes::ofb_crypt},
    {"CTR", false, true, modes::ctr_crypt},
};
/// Jumlah mode.
constexpr size_t N_MODES = sizeof(MODES) / sizeof(MODES[0]);

/// Panjang pesan uji avalanche dalam blok.
constexpr size_t AV_BLOCKS = 64;

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
 * @brief Enkripsi plaintext dengan satu mode (dipad dulu jika mode membutuhkannya).
 * @param[in] m  Mode.
 * @param[in] c  Cipher yang sudah dikunci.
 * @param[in] iv IV / counter awal (diabaikan ECB).
 * @param[in] pt Plaintext.
 * @return Ciphertext mentah.
 */
static util::Bytes encrypt(const ModeInfo &m, const BlockCipher &c, const uint8_t *iv,
                           const util::Bytes &pt) {
  return m.enc(c, iv, m.pad ? pkcs7_pad(pt) : pt);
}

/**
 * @brief Hitung jumlah bit yang berbeda antara dua buffer (jarak Hamming).
 * @param[in] a Buffer pertama.
 * @param[in] b Buffer kedua.
 * @param[in] n Panjang kedua buffer dalam byte.
 * @return Jumlah bit yang berbeda.
 */
static long diff_bits(const uint8_t *a, const uint8_t *b, size_t n) {
  long bits = 0;
  for (size_t i = 0; i < n; i++) bits += __builtin_popcount((unsigned)(a[i] ^ b[i]));
  return bits;
}

/**
 * @brief Statistik sederhana (rata-rata, min, max) untuk persentase.
 */
struct Acc {
  double sum = 0;   ///< Jumlah seluruh nilai.
  double mn = 1e9;  ///< Nilai terkecil.
  double mx = -1e9; ///< Nilai terbesar.
  long n = 0;       ///< Banyaknya sampel.

  /**
   * @brief Catat satu sampel.
   * @param[in] v Nilai sampel.
   */
  void add(double v) {
    sum += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    n++;
  }
  /// @return Rata-rata, atau 0 jika belum ada sampel.
  double mean() const { return n ? sum / n : 0; }
};

/**
 * @brief Tulis satu baris CSV avalanche.
 * @param[in] csv    File CSV.
 * @param[in] mode   Nama mode.
 * @param[in] what   Bit yang dibalik (plaintext, kunci, iv).
 * @param[in] scope  Bagian ciphertext yang diukur.
 * @param[in] a      Statistik.
 */
static void csv_av(FILE *csv, const char *mode, const char *what, const char *scope, const Acc &a) {
  if (a.n == 0)
    fprintf(csv, "%s,%s,%s,,,,0\n", mode, what, scope);
  else
    fprintf(csv, "%s,%s,%s,%.4f,%.4f,%.4f,%ld\n", mode, what, scope, a.mean(), a.mn, a.mx, a.n);
}

/**
 * @brief Bagian 1: bukti padding PKCS#7 dan panjang ciphertext tiap mode.
 */
static void padding_demo() {
  printf("=== 1. Bukti padding (PKCS#7 untuk ECB/CBC, tanpa padding untuk CFB/OFB/CTR) ===\n");
  printf("Plaintext = n byte 'A' (0x41). Kolom kanan: panjang ciphertext mentah tiap mode.\n\n");
  printf("%4s  %-50s  %4s %4s %4s %4s %4s\n", "n", "blok terakhir setelah pkcs7_pad (hex)", "ECB", "CBC",
         "CFB", "OFB", "CTR");
  uint8_t key[KEY_SIZE] = {0}, iv[BLOCK_SIZE] = {0};
  CustomCipher c(key);
  const size_t lens[] = {0, 1, 15, 16, 17, 31, 32};
  for (size_t n : lens) {
    util::Bytes pt(n, 0x41);
    util::Bytes padded = pkcs7_pad(pt);
    std::string last = util::to_hex(padded.data() + padded.size() - BLOCK_SIZE, BLOCK_SIZE);
    std::string spaced;
    for (size_t i = 0; i < last.size(); i += 2) {
      if (i) spaced += ' ';
      spaced += last.substr(i, 2);
    }
    printf("%4zu  %-50s", n, spaced.c_str());
    for (const auto &m : MODES) printf(" %4zu", encrypt(m, c, iv, pt).size());
    printf("\n");
  }
  printf("\nPadding selalu ditambahkan (1..16 byte), sehingga n kelipatan 16 mendapat 1 blok penuh 0x10.\n\n");
}

/**
 * @brief Bagian 2: avalanche per mode.
 * @param[in] trials Jumlah percobaan per mode.
 * @param[in] csv    File CSV keluaran.
 */
static void avalanche_modes(int trials, FILE *csv) {
  printf("=== 2. Avalanche per mode (pesan %zu blok = %zu byte, %d percobaan per mode) ===\n", AV_BLOCKS,
         AV_BLOCKS * BLOCK_SIZE, trials);
  printf("Satu bit plaintext dibalik di blok k (1 <= k <= %zu); persentase = bit ciphertext berubah.\n\n",
         AV_BLOCKS - 2);
  printf("%-4s | %-28s %-28s %-28s | %-24s | %-24s\n", "", "pt: blok sebelum k", "pt: blok k",
         "pt: blok sesudah k", "1 bit kunci (semua)", "1 bit IV (semua)");
  printf("%-4s | %-28s %-28s %-28s | %-24s | %-24s\n", "mode", "rata2 (min-max)", "rata2 (min-max)",
         "rata2 (min-max)", "rata2 (min-max)", "rata2 (min-max)");
  printf("-----+--------------------------------------------------------------------------------------"
         "------+--------------------------+-------------------------\n");
  fprintf(csv, "mode,bit_dibalik,cakupan,rata_rata_pct,min_pct,max_pct,percobaan\n");

  const size_t len = AV_BLOCKS * BLOCK_SIZE;
  for (const auto &m : MODES) {
    Acc before, same, after, key_all, iv_all;
    for (int t = 0; t < trials; t++) {
      uint8_t key[KEY_SIZE], iv[BLOCK_SIZE];
      util::Bytes pt(len);
      fill_random(key, KEY_SIZE);
      fill_random(iv, BLOCK_SIZE);
      fill_random(pt.data(), len);

      CustomCipher c(key);
      util::Bytes base = encrypt(m, c, iv, pt);

      // 1 bit plaintext di blok k
      size_t k = 1 + next_rand() % (AV_BLOCKS - 2);
      size_t bit = next_rand() % (BLOCK_SIZE * 8);
      util::Bytes pt2 = pt;
      pt2[k * BLOCK_SIZE + bit / 8] ^= (uint8_t)(1u << (bit % 8));
      util::Bytes ct2 = encrypt(m, c, iv, pt2);
      size_t k0 = k * BLOCK_SIZE, k1 = (k + 1) * BLOCK_SIZE;
      before.add(100.0 * diff_bits(base.data(), ct2.data(), k0) / (k0 * 8.0));
      same.add(100.0 * diff_bits(base.data() + k0, ct2.data() + k0, BLOCK_SIZE) / (BLOCK_SIZE * 8.0));
      after.add(100.0 * diff_bits(base.data() + k1, ct2.data() + k1, len - k1) / ((len - k1) * 8.0));

      // 1 bit kunci
      uint8_t key2[KEY_SIZE];
      std::memcpy(key2, key, KEY_SIZE);
      size_t kb = next_rand() % (KEY_SIZE * 8);
      key2[kb / 8] ^= (uint8_t)(1u << (kb % 8));
      CustomCipher c2(key2);
      util::Bytes ctk = encrypt(m, c2, iv, pt);
      key_all.add(100.0 * diff_bits(base.data(), ctk.data(), base.size()) / (base.size() * 8.0));

      // 1 bit IV
      if (m.uses_iv) {
        uint8_t iv2[BLOCK_SIZE];
        std::memcpy(iv2, iv, BLOCK_SIZE);
        size_t ib = next_rand() % (BLOCK_SIZE * 8);
        iv2[ib / 8] ^= (uint8_t)(1u << (ib % 8));
        util::Bytes cti = encrypt(m, c, iv2, pt);
        iv_all.add(100.0 * diff_bits(base.data(), cti.data(), base.size()) / (base.size() * 8.0));
      }
    }

    auto cell = [](const Acc &a) {
      char buf[64];
      if (a.n == 0)
        snprintf(buf, sizeof buf, "tidak dipakai");
      else
        snprintf(buf, sizeof buf, "%6.2f%% (%.2f-%.2f)", a.mean(), a.mn, a.mx);
      return std::string(buf);
    };
    printf("%-4s | %-28s %-28s %-28s | %-24s | %-24s\n", m.name, cell(before).c_str(), cell(same).c_str(),
           cell(after).c_str(), cell(key_all).c_str(), cell(iv_all).c_str());

    csv_av(csv, m.name, "plaintext", "blok_sebelum", before);
    csv_av(csv, m.name, "plaintext", "blok_sama", same);
    csv_av(csv, m.name, "plaintext", "blok_sesudah", after);
    csv_av(csv, m.name, "kunci", "semua", key_all);
    csv_av(csv, m.name, "iv", "semua", iv_all);
  }
  printf("\nCatatan: 1/128 bit = 0,78%%. CFB/OFB/CTR meng-XOR plaintext dengan keystream, sehingga 1 bit\n"
         "plaintext hanya mengubah 1 bit ciphertext di posisi yang sama (CFB lalu merambat ke blok\n"
         "berikutnya lewat feedback). Ini sifat mode, bukan kelemahan cipher.\n\n");
}

/**
 * @brief Buat plaintext uji.
 * @param[in] kind Jenis input: "teks", "nol", atau "pola".
 * @param[in] n    Panjang dalam byte.
 * @return Plaintext.
 */
static util::Bytes make_input(const std::string &kind, size_t n) {
  util::Bytes out(n);
  if (kind == "teks") {
    static const char *para =
        "Kriptografi adalah ilmu sekaligus seni untuk menjaga keamanan pesan. Block cipher "
        "memproses pesan per blok berukuran tetap melalui beberapa putaran substitusi dan "
        "transposisi, sehingga setiap bit ciphertext bergantung pada seluruh bit plaintext dan "
        "kunci. Mode operasi menentukan bagaimana blok-blok tersebut dirangkai, sedangkan "
        "padding mengisi sisa blok terakhir yang tidak penuh.\n";
    size_t plen = std::strlen(para);
    for (size_t i = 0; i < n; i++) out[i] = (uint8_t)para[i % plen];
  } else if (kind == "pola") {
    for (size_t i = 0; i < n; i++) out[i] = (uint8_t)i;
  }
  return out;
}

/**
 * @brief Statistik byte satu buffer.
 */
struct ByteStats {
  double entropy = 0; ///< Entropi Shannon (bit/byte).
  double chi2 = 0;    ///< Statistik chi-square terhadap distribusi seragam.
  double p = 0;       ///< p-value chi-square (df = 255).
  int unique = 0;     ///< Banyaknya nilai byte berbeda yang muncul.
  double blocks = 0;  ///< Persentase blok 16 byte yang unik.
};

/**
 * @brief p-value uji chi-square, aproksimasi Wilson-Hilferty.
 * @param[in] chi2 Statistik chi-square.
 * @param[in] df   Derajat kebebasan.
 * @return Peluang P(X >= chi2) di bawah H0.
 */
static double chi2_pvalue(double chi2, double df) {
  double z = (std::cbrt(chi2 / df) - (1.0 - 2.0 / (9.0 * df))) / std::sqrt(2.0 / (9.0 * df));
  return 0.5 * std::erfc(z / std::sqrt(2.0));
}

/**
 * @brief Hitung entropi, chi-square, jumlah byte unik, dan keunikan blok.
 * @param[in]  d      Data.
 * @param[out] counts Frekuensi tiap nilai byte (256 elemen).
 * @return Statistik.
 */
static ByteStats byte_stats(const util::Bytes &d, uint64_t counts[256]) {
  ByteStats s;
  std::memset(counts, 0, 256 * sizeof(uint64_t));
  for (uint8_t b : d) counts[b]++;
  const double n = (double)d.size(), e = n / 256.0;
  for (int b = 0; b < 256; b++) {
    if (counts[b]) {
      double p = counts[b] / n;
      s.entropy -= p * std::log2(p);
      s.unique++;
    }
    double diff = counts[b] - e;
    s.chi2 += diff * diff / e;
  }
  s.p = chi2_pvalue(s.chi2, 255.0);

  std::set<std::array<uint8_t, BLOCK_SIZE>> seen;
  size_t nb = d.size() / BLOCK_SIZE;
  for (size_t i = 0; i < nb; i++) {
    std::array<uint8_t, BLOCK_SIZE> blk;
    std::memcpy(blk.data(), d.data() + i * BLOCK_SIZE, BLOCK_SIZE);
    seen.insert(blk);
  }
  s.blocks = nb ? 100.0 * seen.size() / nb : 0;
  return s;
}

/**
 * @brief Bagian 3 dan 4: entropi, chi-square, keunikan blok, dan histogram.
 * @param[in] runs    Jumlah pasangan kunci/IV.
 * @param[in] size    Panjang tiap plaintext dalam byte.
 * @param[in] out_dir Direktori keluaran.
 * @param[in] csv     File CSV ringkasan entropi.
 */
static void entropy_modes(int runs, size_t size, const fs::path &out_dir, FILE *csv) {
  printf("=== 3. Entropi, chi-square, dan keunikan blok (%zu KiB per input, %d kunci/IV) ===\n",
         size / 1024, runs);
  printf("Entropi maks 8 bit/byte. Chi-square df=255: seragam jika ~255 +- 45 (lolos jika p >= 0,05).\n"
         "Blok unik = persentase blok 16 byte yang tidak berulang.\n\n");
  fprintf(csv, "input,mode,ulangan,byte,entropi_rata2,entropi_min,chi2_rata2,chi2_min,chi2_max,"
               "p_rata2,lolos_5pct,byte_unik_min,blok_unik_pct_rata2\n");

  const char *inputs[] = {"teks", "nol", "pola"};
  for (const char *kind : inputs) {
    util::Bytes pt = make_input(kind, size);
    uint64_t pt_counts[256];
    ByteStats ps = byte_stats(pt, pt_counts);

    printf("Input \"%s\":\n", kind);
    printf("  %-6s %14s %14s %34s %8s %10s %10s\n", "", "entropi", "entropi min", "chi2 rata2 (min-max)",
           "lolos5%", "byte unik", "blok unik");
    printf("  %-6s %14.4f %14s %34.1f %8s %10d %9.2f%%\n", "plain", ps.entropy, "-", ps.chi2, "-",
           ps.unique, ps.blocks);
    fprintf(csv, "%s,PLAIN,1,%zu,%.6f,%.6f,%.2f,%.2f,%.2f,%.6g,%d,%d,%.4f\n", kind, pt.size(), ps.entropy,
            ps.entropy, ps.chi2, ps.chi2, ps.chi2, ps.p, ps.p >= 0.05 ? 1 : 0, ps.unique, ps.blocks);

    std::vector<std::array<uint64_t, 256>> hist(N_MODES);
    for (size_t mi = 0; mi < N_MODES; mi++) {
      const auto &m = MODES[mi];
      Acc ent, chi, pv, blk;
      int pass = 0, uniq_min = 256;
      size_t ct_len = 0;
      for (int r = 0; r < runs; r++) {
        uint8_t key[KEY_SIZE], iv[BLOCK_SIZE];
        fill_random(key, KEY_SIZE);
        fill_random(iv, BLOCK_SIZE);
        CustomCipher c(key);
        util::Bytes ct = encrypt(m, c, iv, pt);
        ct_len = ct.size();
        uint64_t counts[256];
        ByteStats s = byte_stats(ct, counts);
        if (r == 0) std::memcpy(hist[mi].data(), counts, sizeof(counts));
        ent.add(s.entropy);
        chi.add(s.chi2);
        pv.add(s.p);
        blk.add(s.blocks);
        if (s.p >= 0.05) pass++;
        if (s.unique < uniq_min) uniq_min = s.unique;
      }
      char chi_cell[48], pass_cell[16];
      snprintf(chi_cell, sizeof chi_cell, "%.1f (%.1f-%.1f)", chi.mean(), chi.mn, chi.mx);
      snprintf(pass_cell, sizeof pass_cell, "%d/%d", pass, runs);
      printf("  %-6s %14.4f %14.4f %34s %8s %10d %9.2f%%\n", m.name, ent.mean(), ent.mn, chi_cell, pass_cell,
             uniq_min, blk.mean());
      fprintf(csv, "%s,%s,%d,%zu,%.6f,%.6f,%.2f,%.2f,%.2f,%.6g,%d,%d,%.4f\n", kind, m.name, runs, ct_len,
              ent.mean(), ent.mn, chi.mean(), chi.mn, chi.mx, pv.mean(), pass, uniq_min, blk.mean());
    }
    printf("\n");

    fs::path hpath = out_dir / (std::string("hist_") + kind + ".csv");
    FILE *h = std::fopen(hpath.string().c_str(), "w");
    if (!h) {
      std::fprintf(stderr, "Gagal menulis %s\n", hpath.string().c_str());
      std::exit(1);
    }
    fprintf(h, "byte,PLAIN");
    for (const auto &m : MODES) fprintf(h, ",%s", m.name);
    fprintf(h, "\n");
    for (int b = 0; b < 256; b++) {
      fprintf(h, "%d,%llu", b, (unsigned long long)pt_counts[b]);
      for (size_t mi = 0; mi < N_MODES; mi++) fprintf(h, ",%llu", (unsigned long long)hist[mi][b]);
      fprintf(h, "\n");
    }
    std::fclose(h);
  }
  printf("Histogram (run pertama) disimpan sebagai hist_<input>.csv di %s\n", out_dir.string().c_str());
}

/**
 * @brief Buka file CSV untuk ditulis, keluar jika gagal.
 * @param[in] p Path file.
 * @return Pointer FILE.
 */
static FILE *open_csv(const fs::path &p) {
  FILE *f = std::fopen(p.string().c_str(), "w");
  if (!f) {
    std::fprintf(stderr, "Gagal menulis %s\n", p.string().c_str());
    std::exit(1);
  }
  return f;
}

/**
 * @brief Jalankan seluruh analisis per mode.
 * @param[in] argc Jumlah argumen.
 * @param[in] argv Argumen (lihat opsi di deskripsi file).
 * @return 0 jika selesai, 1 jika argumen salah atau gagal menulis berkas.
 */
int main(int argc, char **argv) {
  fs::path out_dir = "test-results/data";
  int trials = 500, runs = 10;
  size_t size_kib = 1024;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto need = [&]() -> const char * {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "Opsi %s butuh nilai\n", a.c_str());
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--out") out_dir = need();
    else if (a == "--trials") trials = std::atoi(need());
    else if (a == "--runs") runs = std::atoi(need());
    else if (a == "--size") size_kib = (size_t)std::atoll(need());
    else {
      std::fprintf(stderr, "Opsi tidak dikenal: %s\n"
                           "Pakai: mode_analysis [--out DIR] [--trials N] [--runs N] [--size KIB]\n",
                   a.c_str());
      return 1;
    }
  }
  if (trials < 1 || runs < 1 || size_kib < 1) {
    std::fprintf(stderr, "--trials, --runs, dan --size harus >= 1\n");
    return 1;
  }

  std::error_code ec;
  fs::create_directories(out_dir, ec);
  if (ec) {
    std::fprintf(stderr, "Gagal membuat direktori %s: %s\n", out_dir.string().c_str(), ec.message().c_str());
    return 1;
  }

  padding_demo();

  FILE *av = open_csv(out_dir / "avalanche_modes.csv");
  avalanche_modes(trials, av);
  std::fclose(av);

  FILE *en = open_csv(out_dir / "entropy_modes.csv");
  entropy_modes(runs, size_kib * 1024, out_dir, en);
  std::fclose(en);

  printf("CSV ringkasan: %s, %s\n", (out_dir / "avalanche_modes.csv").string().c_str(),
         (out_dir / "entropy_modes.csv").string().c_str());
  return 0;
}
