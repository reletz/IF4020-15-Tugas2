// src/core/cipher.cpp (DUMMY)
#include "cipher.hpp"

CustomCipher::CustomCipher(const uint8_t master_key[KEY_SIZE]) {
    uint64_t k = 0;
    for (int i = 0; i < 8; i++) k = (k << 8) | master_key[i];
    for (int i = 0; i < ROUNDS; i++) rk_[i] = k;
}

void CustomCipher::encrypt_block(const uint8_t *in, uint8_t *out) const {
    for (size_t i = 0; i < BLOCK_SIZE; i++)
        out[i] = in[i] ^ (uint8_t)(rk_[0] >> (8 * (i % 8)));
}

void CustomCipher::decrypt_block(const uint8_t *in, uint8_t *out) const {
    encrypt_block(in, out);   // XOR dibalik dengan XOR yang sama
}