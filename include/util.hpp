#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file util.hpp
 * @brief Fungsi utilitas pembantu untuk operasi byte, heksadesimal, I/O berkas, dan keacakan kriptografis.
 */

namespace util {

/// Alias untuk representasi urutan byte dalam memori.
using Bytes = std::vector<uint8_t>;

/**
 * @brief Mengisi buffer dengan byte acak kriptografis yang aman dari sistem operasi.
 *
 * Mengambil entropi langsung dari kernel OS (getentropy / /dev/urandom).
 * Tidak menggunakan rand(), mt19937, atau generator pseudo-random berbasis waktu.
 *
 * @param[out] out Pointer ke buffer tujuan.
 * @param[in]  len Jumlah byte acak yang diminta.
 * @throw std::invalid_argument jika out bernilai nullptr dan len > 0.
 * @throw std::runtime_error jika pemanggilan entropy sistem operasi gagal.
 */
void secure_random(uint8_t* out, size_t len);

/**
 * @brief Menghasilkan vektor berisi byte acak kriptografis yang aman dari sistem operasi.
 *
 * @param[in] len Panjang vektor byte acak yang diinginkan.
 * @return Bytes Vektor berisi byte acak berukuran len.
 * @throw std::runtime_error jika pemanggilan entropy sistem operasi gagal.
 */
Bytes secure_random(size_t len);

/**
 * @brief Mengonversi deretan byte menjadi representasi string heksadesimal huruf kecil (lowercase).
 *
 * @param[in] data Pointer ke data byte sumber.
 * @param[in] len  Jumlah byte yang akan dikonversi.
 * @return std::string Representasi string heksadesimal (2 karakter per byte).
 */
std::string to_hex(const uint8_t* data, size_t len);

/**
 * @brief Helper overload untuk mengonversi objek Bytes menjadi string heksadesimal.
 *
 * @param[in] data Vektor byte sumber.
 * @return std::string Representasi string heksadesimal.
 */
std::string to_hex(const Bytes& data);

/**
 * @brief Mengonversi string heksadesimal menjadi deretan byte (Bytes).
 *
 * Mendukung karakter heksadesimal huruf besar maupun huruf kecil ('0'-'9', 'a'-'f', 'A'-'F').
 *
 * @param[in] hex String heksadesimal sumber.
 * @return Bytes Vektor hasil konversi byte.
 * @throw std::invalid_argument jika panjang string ganjil atau terdapat karakter non-heksadesimal.
 */
Bytes from_hex(const std::string& hex);

/**
 * @brief Membaca seluruh isi berkas secara biner.
 *
 * Mendukung berkas berukuran 0 byte (mengembalikan vektor kosong) hingga berkas besar.
 *
 * @param[in] path Jalur/path berkas yang akan dibaca.
 * @return Bytes Vektor byte berisi seluruh konten berkas.
 * @throw std::runtime_error jika berkas tidak dapat dibuka, dibaca, atau tidak ditemukan (path disertakan dalam pesan).
 */
Bytes read_file(const std::string& path);

/**
 * @brief Menuliskan seluruh isi byte ke berkas secara atomik (atomic-ish).
 *
 * Menulis data terlebih dahulu ke berkas sementara "<path>.tmp", kemudian me-rename
 * ke jalur target. Jika terjadi kegagalan penulisan, berkas sementara akan dihapus
 * dan exception dilempar sehingga tidak meninggalkan berkas corrupt/separuh tulis.
 *
 * @param[in] path Jalur/path berkas target.
 * @param[in] data Data byte yang akan ditulis.
 * @throw std::runtime_error jika berkas sementara gagal dibuat, ditulis, atau di-rename.
 */
void write_file(const std::string& path, const Bytes& data);

/**
 * @brief Melakukan operasi XOR bitwise antara dua blok byte: out[i] = a[i] ^ b[i].
 *
 * Aman digunakan secara in-place (misalnya out sama dengan a atau b).
 *
 * @param[in]  a   Pointer ke blok data pertama.
 * @param[in]  b   Pointer ke blok data kedua.
 * @param[out] out Pointer ke buffer tujuan hasil XOR.
 * @param[in]  len Panjang byte yang di-XOR.
 * @throw std::invalid_argument jika a, b, atau out bernilai nullptr saat len > 0.
 */
void xor_block(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t len);

/**
 * @brief Membandingkan dua buffer byte dalam waktu konstan (constant time).
 *
 * Tidak melakukan short-circuit / early exit ketika perbedaan ditemukan,
 * untuk memitigasi serangan timing attack pada verifikasi tag MAC / rahasia.
 *
 * @param[in] a   Pointer ke buffer data pertama.
 * @param[in] b   Pointer ke buffer data kedua.
 * @param[in] len Jumlah byte yang dibandingkan.
 * @return true jika kedua buffer bernilai identik sepanjang len byte, false jika berbeda.
 */
bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len);

/**
 * @brief Membersihkan memori dengan mengisi byte nol (0) menggunakan volatile pointer.
 *
 * Mencegah kompilator mengoptimasi (menghilangkan/dead-store elimination) operasi
 * pembersihan buffer yang memuat materi kunci atau plaintext sensitif.
 *
 * @param[out] p   Pointer ke memori yang akan dibersihkan.
 * @param[in]  len Panjang memori dalam byte.
 */
void secure_zero(void* p, size_t len);

} // namespace util

#endif // UTIL_HPP
