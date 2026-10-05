#include "key_schedule.hpp"
#include "bits.hpp"
#include "sbox.hpp"
#include "cipher.hpp"
#include <utility>

static constexpr uint64_t PHI = 0x9E3779B97F4A7C15ULL;

void key_schedule(uint64_t a, uint64_t b, uint64_t rk[ROUNDS]){
    for (int i = 0; i < ROUNDS; i++){
        a = rotl64(a, 13) + PHI * (uint64_t)(i + 1);
        a = sbox_bytes(a);
        b ^= rotl64(a, 29);
        std::swap(a, b);
        rk[i] = a ^ b;
    }
}