# Dokumentasi API NAMA_ALGORITMA {#mainpage}

Dokumentasi ini menjelaskan API C++ block cipher **NAMA_ALGORITMA** (Tugas 2 IF4020 Kriptografi,
Kelompok 15): cipher blok 128-bit, lima mode operasi, padding PKCS#7, autentikasi CMAC, dan format
kontainer berkas yang dipakai aplikasi `cipher_cli`. Semua komponen kriptografi diimplementasikan
sendiri tanpa pustaka kriptografi pihak ketiga.

Halaman per fungsi dan kelas dibangkitkan otomatis dari komentar Doxygen di `include/*.hpp`
(lihat menu **Files** dan **Classes**). Halaman ini merangkum cara memakainya.

## 1. Deskripsi API

API tersusun dalam empat lapisan. Lapisan atas hanya bergantung pada lapisan di bawahnya.

| Lapisan | Header | Isi |
|---|---|---|
| Aplikasi | `src/main.cpp` | CLI `cipher_cli` (encrypt, decrypt, keygen) |
| Kontainer & integritas | @ref container.hpp, @ref mac.hpp | @ref SecureEnvelope (Encrypt-then-MAC), @ref Cmac, @ref derive_keys |
| Mode operasi & padding | @ref modes.hpp, @ref padding.hpp | `modes::ecb_*`, `cbc_*`, `cfb_*`, `ofb_crypt`, `ctr_crypt`, @ref pkcs7_pad, @ref pkcs7_unpad |
| Inti cipher | @ref cipher.hpp, @ref round.hpp, @ref key_schedule.hpp, @ref sbox.hpp, @ref permutation.hpp, @ref bits.hpp | @ref BlockCipher, @ref CustomCipher, @ref round_f, @ref key_schedule |
| Utilitas | @ref util.hpp | `util::Bytes`, hex, XOR, CSPRNG OS, I/O berkas, `secure_zero` |

Parameter cipher:

| Parameter | Nilai | Konstanta |
|---|---|---|
| Ukuran blok | 128 bit (16 byte) | @ref BLOCK_SIZE |
| Ukuran kunci | 128 bit (16 byte) | @ref KEY_SIZE |
| Jumlah round | 16 (Feistel 2 cabang, 64 bit per cabang) | @ref ROUNDS |
| Tag MAC | 128 bit (16 byte) | @ref MAC_TAG_SIZE |

## 2. Parameter masukan dan keluaran

Tipe data umum: `util::Bytes` = `std::vector<uint8_t>`. Pointer `const uint8_t*` selalu menunjuk
ke buffer dengan panjang tetap yang disebut di tabel.

### Inti cipher

| Fungsi | Masukan | Keluaran |
|---|---|---|
| `CustomCipher(const uint8_t master_key[16])` | master key 16 byte | objek cipher (round key dibangkitkan sekali) |
| `void encrypt_block(const uint8_t* in, uint8_t* out) const` | `in`: plaintext 16 byte | `out`: ciphertext 16 byte (boleh sama dengan `in`) |
| `void decrypt_block(const uint8_t* in, uint8_t* out) const` | `in`: ciphertext 16 byte | `out`: plaintext 16 byte |
| `uint64_t round_f(uint64_t r, uint64_t k)` | setengah blok 64 bit, round key 64 bit | masker 64 bit |
| `void key_schedule(uint64_t a, uint64_t b, uint64_t rk[16])` | dua separuh master key | 16 round key 64 bit |

### Mode operasi (namespace `modes`)

Semua fungsi mode punya signature yang sama:

    util::Bytes f(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in);

| Fungsi | `iv` | Syarat `in` | Panjang keluaran | Exception |
|---|---|---|---|---|
| `ecb_encrypt`, `ecb_decrypt` | diabaikan (boleh `nullptr`) | kelipatan 16 | = `in.size()` | `std::invalid_argument` |
| `cbc_encrypt`, `cbc_decrypt` | 16 byte | kelipatan 16 | = `in.size()` | `std::invalid_argument` |
| `cfb_encrypt`, `cfb_decrypt` | 16 byte | bebas | = `in.size()` | `std::invalid_argument` jika `iv` null |
| `ofb_crypt` (= `ofb_encrypt`/`ofb_decrypt`) | 16 byte | bebas | = `in.size()` | idem |
| `ctr_crypt` (= `ctr_encrypt`/`ctr_decrypt`) | counter awal 16 byte | bebas | = `in.size()` | idem |
| `ctr_at(iv, i, out)` | counter awal | indeks blok `uint64_t` | `out` = (IV + i) mod 2^128 | `std::invalid_argument` |

ECB dan CBC membutuhkan input yang sudah dipad. CFB, OFB, dan CTR bekerja seperti stream cipher
(tanpa padding). Untuk data ≥ @ref modes::PARALLEL_THRESHOLD_BLOCKS blok, ECB, CTR, dan dekripsi
CBC/CFB dijalankan paralel dengan OpenMP. Hasilnya identik dengan versi serial.

### Padding

| Fungsi | Masukan | Keluaran | Exception |
|---|---|---|---|
| `util::Bytes pkcs7_pad(const util::Bytes& in)` | data sembarang | panjang kelipatan 16, selalu bertambah 1..16 byte | - |
| `util::Bytes pkcs7_unpad(const util::Bytes& in)` | data berpadding | data asli | @ref PaddingError |

### Integritas dan kontainer

| Fungsi | Masukan | Keluaran | Exception |
|---|---|---|---|
| `SecureEnvelope(const uint8_t master_key[16])` | master key | objek (kunci enkripsi dan kunci MAC diturunkan dari master key) | - |
| `util::Bytes seal(const util::Bytes& pt, Mode mode, const uint8_t* iv = nullptr) const` | plaintext, @ref Mode, IV opsional (null = acak dari CSPRNG OS) | kontainer biner | `std::runtime_error` jika CSPRNG gagal |
| `util::Bytes open(const util::Bytes& blob) const` | kontainer | plaintext | @ref FormatError, @ref AuthenticationError |
| `static void Cmac::compute(cipher, data, len, tag)` | data sembarang | tag 16 byte | `std::invalid_argument` |
| `static bool Cmac::verify(cipher, data, len, tag)` | data, tag 16 byte | `true` jika cocok (constant-time) | `std::invalid_argument` |
| `DerivedKeys derive_keys(const BlockCipher& master)` | cipher berkunci master key | `enc = E(0..01)`, `mac = E(0..02)` | - |

Format kontainer v1 (keluaran `seal`, masukan `open`):

| Offset | Panjang | Isi |
|---|---|---|
| 0 | 4 | magic `IF15` |
| 4 | 1 | versi `0x01` |
| 5 | 1 | mode: 1 ECB, 2 CBC, 3 CFB, 4 OFB, 5 CTR |
| 6 | 16 | IV / counter awal (nol untuk ECB) |
| 22 | m | ciphertext (dipad PKCS#7 untuk ECB/CBC) |
| 22 + m | 16 | tag CMAC atas byte 0 .. 22 + m − 1 |

## 3. Contoh penggunaan

### Satu blok

@code
#include "cipher.hpp"

uint8_t key[KEY_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
uint8_t pt[BLOCK_SIZE] = {'h', 'e', 'l', 'l', 'o'};
uint8_t ct[BLOCK_SIZE], back[BLOCK_SIZE];

CustomCipher cipher(key);
cipher.encrypt_block(pt, ct);
cipher.decrypt_block(ct, back);   // back == pt
@endcode

### Mode operasi dan padding

@code
#include "modes.hpp"
#include "padding.hpp"

CustomCipher cipher(key);
uint8_t iv[BLOCK_SIZE];
util::secure_random(iv, BLOCK_SIZE);

util::Bytes msg = {'P', 'e', 's', 'a', 'n'};

// CBC: pad dulu, lalu unpad setelah dekripsi
util::Bytes ct  = modes::cbc_encrypt(cipher, iv, pkcs7_pad(msg));
util::Bytes out = pkcs7_unpad(modes::cbc_decrypt(cipher, iv, ct));   // out == msg

// CTR: tanpa padding, fungsi yang sama untuk enkripsi dan dekripsi
util::Bytes c2 = modes::ctr_crypt(cipher, iv, msg);                  // c2.size() == msg.size()
util::Bytes p2 = modes::ctr_crypt(cipher, iv, c2);                   // p2 == msg
@endcode

### Kontainer terautentikasi (yang dipakai CLI)

@code
#include "container.hpp"

SecureEnvelope env(key);
util::Bytes blob = env.seal(util::read_file("dokumen.pdf"), Mode::CBC);
util::write_file("dokumen.enc", blob);

try {
    util::Bytes plain = env.open(util::read_file("dokumen.enc"));
} catch (const AuthenticationError&) {
    // tag tidak cocok: berkas diubah atau kunci salah
} catch (const FormatError&) {
    // bukan kontainer v1 yang valid, atau padding rusak
}
@endcode

### CLI

    cipher_cli keygen -o secret.key
    cipher_cli encrypt -m cbc --key-file secret.key -i dokumen.pdf -o dokumen.enc
    cipher_cli encrypt -m ctr -k 000102030405060708090a0b0c0d0e0f --iv 0f0e0d0c0b0a09080706050403020100 -t "Pesan" -o pesan.enc
    cipher_cli decrypt --key-file secret.key -i dokumen.enc -o dokumen.pdf

Exit code: `0` sukses, `1` argumen salah, `2` I/O, `3` format/padding tidak valid, `4` integritas gagal.

## 4. Penjelasan tambahan

- **Encrypt-then-MAC.** @ref SecureEnvelope::open memverifikasi tag CMAC sebelum mendekripsi dan
  melepas padding, sehingga tidak ada padding oracle. Kunci salah juga terdeteksi sebagai
  @ref AuthenticationError.
- **Pemisahan kunci.** Master key tidak dipakai langsung untuk data: @ref derive_keys menurunkan
  kunci enkripsi dan kunci MAC yang berbeda.
- **IV.** Gunakan IV acak (default `seal`) untuk setiap enkripsi. Memakai ulang IV dengan kunci yang
  sama membocorkan informasi, terutama pada CTR dan OFB. Parameter IV kustom disediakan untuk
  pengujian deterministik.
- **Thread safety.** @ref CustomCipher immutable setelah dibuat, sehingga aman dibaca dari banyak
  thread. @ref Cmac menyimpan state dan tidak boleh dipakai bersama antar thread.
- **Memori sensitif.** Buffer kunci dan state MAC di-nolkan dengan `util::secure_zero` setelah dipakai.
- **Platform.** Linux / WSL (memakai `getentropy`, POSIX I/O). Build: `make`, dokumentasi ini: `make docs`.
- **Alat analisis.** `analysis/avalanche.cpp`, `analysis/sac.cpp`, `analysis/rounds.cpp`,
  `analysis/mode_analysis.cpp`, dan `analysis/bench_modes.cpp` didokumentasikan di menu **Files**.
  Hasilnya tersimpan di folder `test-results/` repositori.
