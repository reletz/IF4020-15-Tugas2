#include "sbox.hpp"

const uint8_t SBOX[256] = {
#include "sbox_table.inc"
};

uint8_t sbox(uint8_t x) { return SBOX[x]; }

uint32_t sbox32(uint32_t acc) {
    return ((uint32_t)SBOX[(acc >> 24) & 0xFF] << 24)
         | ((uint32_t)SBOX[(acc >> 16) & 0xFF] << 16)
         | ((uint32_t)SBOX[(acc >> 8) & 0xFF] << 8)
         | (uint32_t)SBOX[acc & 0xFF];
}
