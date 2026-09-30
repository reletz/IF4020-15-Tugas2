// src/core/cipher.cpp (DUMMY)
#include "cipher.hpp"
#include "round.hpp"

static uint64_t load64(const uint8_t* p){
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

static void store64(uint8_t *p, uint64_t v){
    for (size_t i = 8; i-- > 0; v >>= 8) p[i] = (uint8_t)(v);
}

CustomCipher::CustomCipher(const uint8_t master_key[KEY_SIZE]) {
    uint64_t k = load64(master_key);
    for (size_t i = 0; i < ROUNDS; i++) rk_[i] = k;

    // sementara aja, round key = 1st half of master key
}

void CustomCipher::encrypt_block(const uint8_t *in, uint8_t *out) const {
    uint64_t L = load64(in), R = load64(in + 8);
    for (size_t i = 0; i < ROUNDS; i++){
        uint64_t next = L ^ round_f(R, rk_[i]);
        L = R;
        R = next;
    } 
    // Last swap
    store64(out, R);
    store64(out + 8, L);
}

void CustomCipher::decrypt_block(const uint8_t *in, uint8_t *out) const {
    uint64_t L = load64(in), R = load64(in + 8);
    for (size_t i = 0; i < ROUNDS; i++){
        uint64_t next = L ^ round_f(R, rk_[ROUNDS - i - 1]);
        L = R;
        R = next;
    } 
    // Last swap
    store64(out, R);
    store64(out + 8, L);
}