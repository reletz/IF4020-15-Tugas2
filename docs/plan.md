euh kurang lebih gini cipher blocknya

### 0. Problem statement
Bikin Block Cipher

Alurnya:
File input -> Padding -> Slice per n byte (bikin 1 blok, 8 x n bit) -> (pilih mode <-> encrypt_block) -> output

Cara naif: bikin E(P) = C, terus buat dekripsi butuh E^-1(C) = P
-> masalahnya SETIAP langkah di E harus bisa dibalik
-> repot, dan banyak operasi, yg ngacak banget, justru gabisa dibalik (misal buang bit, loop yg nyimpen state)
-> butuh struktur yg ga maksa kita bikin kebalikan dari fungsi pengacaknya

### 1. Constraint dari spesifikasi
1. Ukuran blok dibebaskan, dengan ukuran blok minimum adalah sebesar 64 bit.
2. Panjang kunci utama (master key) minimum adalah sepanjang ukuran blok.
3. Algoritma dapat beroperasi pada tingkat bit, byte, atau heksadesimal.
4. Penggunaan jaringan Feistel (Feistel network) bersifat opsional, disesuaikan dengan rancangan skema enkripsi dan dekripsi.
5. Proses enciphering wajib memenuhi prinsip confusion dan diffusion.
6. Diwajibkan untuk menerapkan minimal 2 teknik dasar, yaitu substitusi bit/byte berbasis table lookup (Kotak-S / S-box) dan transposisi (permutasi).
7. Gunakan skema iterated cipher sebanyak n putaran (rounds). Setiap putaran (rounds) wajib menggunakan kunci putaran (round key) yang dibangkitkan dari kunci utama (master key).
8. Diwajibkan untuk menerapkan minimal 1 teknik operasi tambahan di luar substitusi dan transposisi (seperti penjumlahan modulo, kompresi, ekspansi bit, atau rotasi) untuk meningkatkan kompleksitas algoritma.
9. Diizinkan untuk mengimplementasikan fitur kriptografi tambahan di luar ketentuan di atas. Silakan buat algoritma Anda dengan sekompleks mungkin.
10. Berikan nama algoritma block cipher Anda dengan nama yang unik dan kreatif.

tambahan dari spek
- 5 mode: ECB, CBC, CFB, OFB, CTR
- padding + integritas data
- input teks / file biner apa aja (kerja per byte)
- ga boleh pake library kripto pihak ketiga
- nyontek AES/DES/Blowfish = 0

### 2. Coba dirumusin
- ambil Cpp
  - inti cipher: operasi per blok tetap gaya C (uint8_t, array, tanpa alokasi)
  - dibungkus interface `BlockCipher` (encrypt_block/decrypt_block) + `CustomCipher`
    -> mode ECB/CBC/CFB/OFB/CTR cukup nerima `BlockCipher&`, ga peduli cipher-nya apa
  - I/O file, padding, CLI pake vector/string biar ga ribet malloc
  - dokumentasi API (bonus): komentar Doxygen (`///`, `@param`, `@code`) di header
    jadi sumber utama, `docs/api.md` isinya overview, format output, contoh, catatan
    -> hosting di markdown-website sendiri, ga perlu generate HTML Doxygen
- 64 bit kedikitan, rentan kolisi di CBC jika data >= 32GB -> [birthday attack](https://sweet32.info/). harusnya 128 aman
  - kunci minimal = blok -> kunci 128 bit juga
- pake feistel 2 cabang -> jawaban buat problem di bagian 0
  - blok 128 dibelah 2: L (64 bit) sama R (64 bit)
  - 1 round:
      L_baru = R
      R_baru = L (XOR) F(R, K)
  - F cuma dipake buat bikin masking yang di XOR
  - dekripsi: hitung F lagi pake R yg sama -> dapet masker yg sama
    -> XOR lagi -> balik ke L awal
    (XOR 2x pake angka yg sama = balik lagi, contoh: 5 (XOR) 3 (XOR) 3 = 5)
  - jadi F ga perlu dibalik, cukup dihitung ulang
    -> F boleh seribet apa aja
  - dekripsi pake kode yg sama kaya enkripsi, cuma kuncinya dipake dari belakang

### 3. Isi F (idenya)
- F = niru cara kerja sigma-delta (ΣΔ) di DAC audio
  - jalan per word satu-satu, sambil bawa "tabungan" (acc)
      acc = acc + (x - feedback)      -> tambah-tambahan mod 2^32
      q   = S(acc XOR K_i[j])         -> lewat S-box, kunci ikut masuk. (S-box 8 bit, acc 32 bit -> S-box dipake ke 4 byte acc satu-satu)
      feedback = q                    -> hasilnya dipake buat word berikutnya
  - jadi tiap word hasilnya kepengaruh semua word sebelumnya
- loop ini nyimpen state (acc, feedback) -> susah dibalik -> makanya harus pake feistel
- masalah yg harus diberesin:
  - efeknya cuma ngalir ke depan (word belakang ga ngaruh ke word depan)
    -> jalanin 2x: maju, terus mundur
  - bit paling kanan hasil penjumlahan itu gampang ditebak (sama aja kaya XOR biasa)
    -> tambahin rotasi antar word biar bitnya pindah posisi
- abis itu: permutasi bit (transposisi)

### 4. Alur akhirnya

File input -> Padding -> Slice per 16 byte (1 blok = 128 bit) -> (mode <-> encrypt_block) -> output (IV + ciphertext + tag MAC)

Rancangan encrypt_block:
- input: 1 blok (16 byte) + round key K_0 ... K_15
- round key dibikin sekali di awal dari master key (key schedule), bukan per blok
  - master key (128 bit) -> rotasi + XOR konstanta φ tiap putaran -> K_0 ... K_15 (masing2 64 bit)
  - detail persisnya: ntaran dah

- langkah:
  1. belah blok: L = 8 byte kiri, R = 8 byte kanan
  2. ulang 16x (i = 0 ... 15):
       L_baru = R
       R_baru = L XOR F(R, K_i)
  3. setelah round terakhir, tuker L sama R sekali lagi
     (biar decrypt_block bisa pake kode yg sama)
  4. gabung L + R -> 16 byte ciphertext

- isi F(R, K_i) (lihat bagian 3):
  1. R dipecah jadi 2 word (32 bit)
  2. pass maju ΣΔ: word 0 -> word 1, pake K_i[0], K_i[1]
  3. rotasi antar word
  4. pass mundur ΣΔ: word 1 -> word 0
  5. permutasi bit
  6. hasil: 64 bit

Rancangan decrypt_block:
- sama persis kaya encrypt_block
- bedanya cuma urutan kunci: K_15, K_14, ... K_0

### 5. Bedain mode, E, sama F (biar ga ketuker)
ada 3 level, dari luar ke dalam:

```
plaintext -> padding -> slice 16 byte -> [MODE] -> E (encrypt_block) -> ciphertext
                                                   └─ 16 round feistel
                                                       └─ F(R, K_i) tiap round
```

- MODE (ECB/CBC/CFB/OFB/CTR)
  - cuma ngatur gimana blok-blok disambung (chaining, feedback, counter)
  - ga peduli isi cipher, cukup manggil `encrypt_block` / `decrypt_block`
  - yg ngurus IV / counter awal
- E = `encrypt_block` / `decrypt_block`
  - cipher blok lengkap: 1 blok (16 byte) masuk, 1 blok keluar
  - isinya 16 round feistel (bagian 4)
  - dipanggil sekali per blok, sama mode
- F = round function
  - ada DI DALAM E, dipanggil 16x per blok (sekali per round)
  - ga perlu dibalik, cukup dihitung ulang (bagian 2)

yg sering ketuker:
- mode ga manggil F, mode manggil E, E yg manggil F
- di laporan pake nama beda: E buat cipher blok, F buat round function
- CFB/OFB/CTR selalu manggil `encrypt_block` (bahkan pas dekripsi), cuma ECB sama CBC yg butuh `decrypt_block`

alur dari sisi user:
- input: plaintext, master key, mode, IV/counter
- master key -> key schedule -> K_0 ... K_15 (sekali di awal, bukan per blok)
- plaintext -> padding -> slice 16 byte -> mode(E) -> output IV + ciphertext + tag MAC
- dekripsi: cek tag MAC dulu, baru dekripsi