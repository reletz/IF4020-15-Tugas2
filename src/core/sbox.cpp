#include "sbox.hpp"

const uint8_t SBOX[256] = {
#include "sbox_table.inc"
};

uint8_t sbox(uint8_t x) {
    return SBOX[x];
}

uint64_t sbox_bytes(uint64_t a) {
    uint64_t r = 0;
    for (int i = 0; i < 8; i++)
        r |= (uint64_t)SBOX[(a >> (8 * i)) & 0xFF] << (8 * i);
    return r;
}

uint32_t sbox_bytes32(uint32_t a) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++)
        r |= (uint32_t)SBOX[(a >> (8 * i)) & 0xFF] << (8 * i);
    return r;
}
