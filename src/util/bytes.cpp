#include "util.hpp"

#include <stdexcept>

namespace util {

void xor_block(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t len) {
    if (len == 0) {
        return;
    }
    if (a == nullptr || b == nullptr || out == nullptr) {
        throw std::invalid_argument("xor_block: pointer null");
    }

    for (size_t i = 0; i < len; ++i) {
        out[i] = a[i] ^ b[i];
    }
}

bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    if (len == 0) {
        return true;
    }
    if (a == nullptr || b == nullptr) {
        return false;
    }

    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff = diff | static_cast<uint8_t>(a[i] ^ b[i]);
    }

    return diff == 0;
}

void secure_zero(void* p, size_t len) {
    if (p == nullptr || len == 0) {
        return;
    }

    volatile uint8_t* vp = static_cast<volatile uint8_t*>(p);
    while (len--) {
        *vp++ = 0;
    }
}

} // namespace util
