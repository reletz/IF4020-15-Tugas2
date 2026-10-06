#ifndef SBOX_HPP
#define SBOX_HPP

#include <cstdint>

/// sbox_bytes:
uint64_t sbox_bytes(uint64_t a);

/// sbox_bytes for 32 bit (1 word)
uint32_t sbox_bytes32(uint32_t a);

#endif