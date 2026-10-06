#include "permutation.hpp"

uint64_t permute_bits(uint64_t x){
    uint64_t y = 0;
    for (int i = 0; i < 64; i++){
        uint64_t bit = (x >> i) & 1;
        y |= bit << ((i * PERM_MULT) % 64);
    }
    return y;
}
