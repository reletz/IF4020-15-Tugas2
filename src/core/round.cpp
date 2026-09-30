#include "round.hpp"

uint64_t round_f(uint64_t r, uint64_t k){
    return r ^ k;
}