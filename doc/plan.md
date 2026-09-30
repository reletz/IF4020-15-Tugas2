euh kurang lebih gini cipher blocknya

### 0. Problem statement
Bikin Block Cipher
Karena ini enkripsi: kalau plaintext diacak jadi ciphertext, yang nerima harus bisa kembaliin persis.

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
  - inti cipher tulis gaya C aja (uint8_t, array, ga usah class)
  - I/O file, padding, CLI pake vector/string biar ga ribet malloc
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
  - loop ini nyimpen state (acc, feedback) -> susah dibalik ->  harus pake feistel
  - dekripsi pake kode yg sama kaya enkripsi, cuma kuncinya dipake dari belakang

### 3. Isi F (idenya)
- F = niru cara kerja sigma-delta (ΣΔ) di ADC audio
  - jalan per word satu-satu, sambil bawa "tabungan" (acc):
      acc = acc + (x - feedback)      -> tambah-tambahan mod 2^32
      q   = S(acc XOR K)              -> lewat S-box, kunci ikut masuk
      feedback = q                    -> hasilnya dipake buat word berikutnya
  - jadi tiap word hasilnya kepengaruh semua word sebelumnya
- loop ini nyimpen state (acc, feedback) -> susah dibalik -> makanya harus pake feistel
- masalah yg harus diberesin:
  - efeknya cuma ngalir ke depan (word belakang ga ngaruh ke word depan)
    -> jalanin 2x: maju, terus mundur
  - bit paling kanan hasil penjumlahan itu gampang ditebak (sama aja kaya XOR biasa)
    -> tambahin rotasi antar word biar bitnya pindah posisi
- abis itu: permutasi bit (transposisi)
