/**
 * @file bench_modes.cpp
 * @brief Benchmark throughput mode operasi, serial (1 thread) vs paralel (OpenMP).
 *
 * Yang diukur hanya fungsi mode di @c modes:: (tanpa I/O, CMAC, atau padding),
 * berbeda dengan benchmark di @c tests/e2e.sh yang mengukur CLI end-to-end.
 *
 * Untuk tiap operasi dan ukuran data, fungsi dijalankan beberapa kali dengan
 * 1 thread lalu dengan semua thread, dan waktu median yang dipakai.
 * Output serial dan paralel juga dibandingkan byte demi byte.
 *
 * Data plaintext, kunci, dan IV deterministik (seed tetap), jadi hasilnya bisa
 * direproduksi.
 *
 * Jalankan dengan:
 * @code
 * make bench                                  # tabel ke stdout
 * make bench BENCH_ARGS="--sizes 1,16 --reps 3"
 * make bench-csv                              # simpan ke docs/report/data/bench.csv
 * @endcode
 *
 * Opsi:
 * - @c --csv          output CSV (info mesin dikirim ke stderr)
 * - @c --reps N       jumlah pengulangan per pengukuran (default 5)
 * - @c --sizes A,B,.. ukuran data dalam MiB (default 1,16,64)
 * - @c --threads N    jumlah thread untuk run paralel (default semua thread)
 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "cipher.hpp"
#include "modes.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

/// Signature fungsi mode, sama dengan yang dipakai container.
using ModeFn = util::Bytes (*)(const BlockCipher &, const uint8_t *, const util::Bytes &);

/// Satu operasi yang dibenchmark.
struct Op {
  const char *mode;  ///< Nama mode.
  const char *dir;   ///< "enc" atau "dec".
  ModeFn fn;         ///< Fungsi yang diukur.
  ModeFn make_input; ///< Kalau tidak nullptr, input = make_input(plaintext) (untuk dekripsi).
  const char *note;  ///< Keterangan paralelisasi.
};

static const Op OPS[] = {
    {"ECB", "enc", modes::ecb_encrypt, nullptr, "paralel"},
    {"ECB", "dec", modes::ecb_decrypt, modes::ecb_encrypt, "paralel"},
    {"CBC", "enc", modes::cbc_encrypt, nullptr, "serial (definisi)"},
    {"CBC", "dec", modes::cbc_decrypt, modes::cbc_encrypt, "paralel"},
    {"CFB", "enc", modes::cfb_encrypt, nullptr, "serial (definisi)"},
    {"CFB", "dec", modes::cfb_decrypt, modes::cfb_encrypt, "paralel"},
    {"OFB", "enc", modes::ofb_crypt, nullptr, "serial (definisi)"},
    {"CTR", "enc", modes::ctr_crypt, nullptr, "paralel"},
};

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
 * @brief Atur jumlah thread OpenMP. Tidak berbuat apa-apa tanpa OpenMP.
 * @param[in] n Jumlah thread.
 */
static void set_threads(int n) {
#ifdef _OPENMP
  omp_set_num_threads(n);
#else
  (void)n;
#endif
}

/**
 * @brief Jalankan @p fn sebanyak @p reps kali dan ambil waktu median.
 * @param[in]  fn     Fungsi mode yang diukur.
 * @param[in]  c      Cipher.
 * @param[in]  iv     IV / counter awal.
 * @param[in]  in     Data masukan.
 * @param[in]  reps   Jumlah pengulangan.
 * @param[out] result Output dari run terakhir (untuk dibandingkan).
 * @return Waktu median dalam detik.
 */
static double time_median(ModeFn fn, const BlockCipher &c, const uint8_t *iv,
                          const util::Bytes &in, int reps, util::Bytes &result) {
  std::vector<double> t;
  for (int r = 0; r < reps; r++) {
    auto t0 = std::chrono::steady_clock::now();
    result = fn(c, iv, in);
    auto t1 = std::chrono::steady_clock::now();
    t.push_back(std::chrono::duration<double>(t1 - t0).count());
  }
  std::sort(t.begin(), t.end());
  return t[t.size() / 2];
}

/**
 * @brief Ambil nama CPU dari /proc/cpuinfo (Linux).
 * @return Nama CPU, atau "unknown" kalau tidak tersedia.
 */
static std::string cpu_name() {
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("model name", 0) == 0) {
      size_t p = line.find(':');
      if (p != std::string::npos) return line.substr(p + 2);
    }
  }
  return "unknown";
}

/**
 * @brief Cetak cara pakai lalu keluar.
 * @param[in] prog Nama program.
 */
static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s [--csv] [--reps N] [--sizes A,B,...] [--threads N]\n", prog);
  exit(2);
}

/**
 * @brief Benchmark semua mode × ukuran, serial vs paralel.
 * @return 0 kalau output serial dan paralel identik di semua kasus, 1 kalau ada yang beda.
 */
int main(int argc, char **argv) {
  bool csv = false;
  int reps = 5;
  std::vector<size_t> sizes_mib = {1, 16, 64};
#ifdef _OPENMP
  int threads = omp_get_max_threads();
#else
  int threads = 1;
#endif

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--csv") {
      csv = true;
    } else if (a == "--reps" && i + 1 < argc) {
      reps = atoi(argv[++i]);
    } else if (a == "--threads" && i + 1 < argc) {
      threads = atoi(argv[++i]);
    } else if (a == "--sizes" && i + 1 < argc) {
      sizes_mib.clear();
      std::string s = argv[++i];
      size_t pos = 0;
      while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        if (comma == std::string::npos) comma = s.size();
        long v = atol(s.substr(pos, comma - pos).c_str());
        if (v <= 0) usage(argv[0]);
        sizes_mib.push_back((size_t)v);
        pos = comma + 1;
      }
    } else {
      usage(argv[0]);
    }
  }
  if (reps < 1 || threads < 1) usage(argv[0]);

  // Info mesin. Pada mode CSV dikirim ke stderr supaya file CSV tetap bersih.
  FILE *info = csv ? stderr : stdout;
  fprintf(info, "Benchmark mode operasi (median dari %d run, tanpa I/O dan CMAC)\n", reps);
  fprintf(info, "CPU      : %s\n", cpu_name().c_str());
#ifdef _OPENMP
  fprintf(info, "Thread   : %d paralel (omp_get_num_procs = %d), OpenMP %d\n",
          threads, omp_get_num_procs(), _OPENMP);
#else
  fprintf(info, "Thread   : 1 (build tanpa OpenMP, kolom paralel == serial)\n");
#endif
#ifdef __VERSION__
  fprintf(info, "Compiler : %s%s\n", __VERSION__,
#ifdef __OPTIMIZE__
          ", optimasi aktif (-O2)"
#else
          ", TANPA optimasi"
#endif
  );
#endif
  fprintf(info, "Threshold: %zu blok (%zu KiB) sebelum paralel aktif\n\n",
          modes::PARALLEL_THRESHOLD_BLOCKS,
          modes::PARALLEL_THRESHOLD_BLOCKS * BLOCK_SIZE / 1024);

  uint8_t key[KEY_SIZE], iv[BLOCK_SIZE];
  fill_random(key, KEY_SIZE);
  fill_random(iv, BLOCK_SIZE);
  CustomCipher cipher(key);

  if (csv) {
    printf("mode,op,size_mib,serial_mibps,parallel_mibps,speedup,threads,identical\n");
  } else {
    printf("%-4s %-3s %8s %14s %15s %8s %7s %7s  %s\n", "mode", "op", "ukuran",
           "serial MiB/s", "paralel MiB/s", "speedup", "thread", "identik", "keterangan");
    printf("------------------------------------------------------------------------------------------\n");
  }

  bool all_identical = true;
  for (size_t mib : sizes_mib) {
    util::Bytes pt(mib * 1024 * 1024);
    fill_random(pt.data(), pt.size());

    for (const Op &op : OPS) {
      // Input dekripsi = ciphertext dari fungsi enkripsi pasangannya.
      util::Bytes in = op.make_input ? op.make_input(cipher, iv, pt) : pt;
      util::Bytes out_serial, out_parallel;

      set_threads(1);
      double ts = time_median(op.fn, cipher, iv, in, reps, out_serial);
      set_threads(threads);
      double tp = time_median(op.fn, cipher, iv, in, reps, out_parallel);

      bool identical = out_serial == out_parallel;
      all_identical = all_identical && identical;
      double s_rate = (double)mib / ts;
      double p_rate = (double)mib / tp;

      if (csv) {
        printf("%s,%s,%zu,%.2f,%.2f,%.2f,%d,%s\n", op.mode, op.dir, mib, s_rate,
               p_rate, ts / tp, threads, identical ? "yes" : "no");
      } else {
        printf("%-4s %-3s %4zu MiB %14.2f %15.2f %7.2fx %7d %7s  %s\n", op.mode,
               op.dir, mib, s_rate, p_rate, ts / tp, threads,
               identical ? "ya" : "TIDAK", op.note);
      }
      fflush(stdout);
    }
  }

  if (!all_identical) {
    fprintf(stderr, "PERINGATAN: ada output paralel yang berbeda dari serial\n");
    return 1;
  }
  return 0;
}
