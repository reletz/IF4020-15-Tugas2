#include "round.hpp"
#include "sbox.hpp"
#include "bits.hpp"
#include "permutation.hpp"

static uint32_t sd_step(uint32_t &acc, uint32_t &feedback, uint32_t x, uint32_t k){
    acc += (x - feedback);
    uint32_t q = sbox_bytes32(acc ^ k);
    feedback = q;
    return q;
}

static void sd_forward(uint32_t &w0, uint32_t &w1, uint32_t k0, uint32_t k1){
    uint32_t acc = 0;
    uint32_t feedback = 0;
    w0 = sd_step(acc, feedback, w0, k0);
    w1 = sd_step(acc, feedback, w1, k1);
}

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
    
    w0 = rotl32(w0, 7);
    w1 = rotl32(w1, 19);

    sd_backward(w0, w1, k0, k1);

    uint64_t w = (uint64_t)w0 << 32 | (uint64_t)w1;
    return permute_bits(w);
}