#include "sbox.hpp"

const uint8_t SBOX[256] = {
#include "sbox_table.inc"
};

uint8_t sbox(uint8_t x) {
    return SBOX[x];
}

uint32_t sbox32(uint32_t a) {
    return ((uint32_t)SBOX[(a >> 24) & 0xFF] << 24)
         | ((uint32_t)SBOX[(a >> 16) & 0xFF] << 16)
         | ((uint32_t)SBOX[(a >> 8) & 0xFF] << 8)
         | (uint32_t)SBOX[a & 0xFF];
}

uint64_t sbox64(uint64_t a) {
    return ((uint64_t)sbox32((uint32_t)(a >> 32)) << 32) | sbox32((uint32_t)a);
}
