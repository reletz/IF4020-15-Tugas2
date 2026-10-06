#include "round.hpp"
#include "sbox.hpp"
#include "bits.hpp"
#include "permutation.hpp"

/**
 * @brief Satu langkah sigma-delta pada satu word.
 *
 * @code
 * acc      = acc + (x - feedback)     // mod 2^32
 * q        = S(acc xor k)             // S-box tiap byte, kunci ikut masuk
 * feedback = q
 * @endcode
 *
 * Penjumlahan memakai @c uint32_t sehingga wrap mod 2^32 (tidak ada overflow
 * bertanda yang undefined).
 *
 * @param[in,out] acc      Akumulator, dibawa ke langkah berikutnya.
 * @param[in,out] feedback Keluaran langkah sebelumnya.
 * @param[in]     x        Word masukan.
 * @param[in]     k        Kunci 32 bit untuk langkah ini.
 * @return Keluaran @c q, juga disimpan ke @c feedback.
 */
static uint32_t sd_step(uint32_t &acc, uint32_t &feedback, uint32_t x, uint32_t k){
    acc += (x - feedback);
    uint32_t q = sbox32(acc ^ k);
    feedback = q;
    return q;
}

/**
 * @brief Pass maju sigma-delta: w0 lalu w1.
 *
 * @c acc dan @c feedback mulai dari 0 dan dibawa dari w0 ke w1, sehingga w1
 * dipengaruhi w0.
 *
 * @param[in,out] w0 Word pertama, diganti hasil langkah dengan @c k0.
 * @param[in,out] w1 Word kedua, diganti hasil langkah dengan @c k1.
 * @param[in]     k0 Kunci untuk w0.
 * @param[in]     k1 Kunci untuk w1.
 */
static void sd_forward(uint32_t &w0, uint32_t &w1, uint32_t k0, uint32_t k1){
    uint32_t acc = 0;
    uint32_t feedback = 0;
    w0 = sd_step(acc, feedback, w0, k0);
    w1 = sd_step(acc, feedback, w1, k1);
}

<<<<<<< HEAD
/**
 * @brief Pass mundur sigma-delta: w1 lalu w0.
 *
 * Arah dibalik supaya w0 juga dipengaruhi w1 (pass maju hanya mengalir ke
 * depan). Kunci ditukar (w1 pakai @c k0, w0 pakai @c k1) sehingga tiap word
 * terkena kedua kunci. @c acc dan @c feedback mulai dari 0 lagi.
 *
 * @param[in,out] w0 Word pertama, diganti hasil langkah dengan @c k1.
 * @param[in,out] w1 Word kedua, diganti hasil langkah dengan @c k0.
 * @param[in]     k0 Kunci untuk w1.
 * @param[in]     k1 Kunci untuk w0.
 */
=======
>>>>>>> 9543fe6 (feat:merge makefile:)
static void sd_backward(uint32_t &w0, uint32_t &w1, uint32_t k0, uint32_t k1){
    uint32_t acc = 0;
    uint32_t feedback = 0;
    w1 = sd_step(acc, feedback, w1, k0);
    w0 = sd_step(acc, feedback, w0, k1);
}

uint64_t round_f(uint64_t r, uint64_t k){
    uint32_t w0 = (uint32_t)(r >> 32);
    uint32_t w1 = (uint32_t)(r);

    uint32_t k0 = (uint32_t)(k >> 32);
    uint32_t k1 = (uint32_t)(k);

    sd_forward(w0, w1, k0, k1);
<<<<<<< HEAD

=======
    
>>>>>>> 9543fe6 (feat:merge makefile:)
    w0 = rotl32(w0, 7);
    w1 = rotl32(w1, 19);

    sd_backward(w0, w1, k0, k1);

    uint64_t w = (uint64_t)w0 << 32 | (uint64_t)w1;
    return permute_bits(w);
<<<<<<< HEAD
}
=======
}
>>>>>>> 9543fe6 (feat:merge makefile:)
