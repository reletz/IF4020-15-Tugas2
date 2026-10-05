#include "util.hpp"

#include <unistd.h>
#include <sys/random.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace util {

void secure_random(uint8_t* out, size_t len) {
    if (len == 0) {
        return;
    }
    if (out == nullptr) {
        throw std::invalid_argument("secure_random: buffer tujuan bernilai nullptr");
    }

    size_t offset = 0;
    while (offset < len) {
        size_t chunk = std::min<size_t>(len - offset, 256);
        if (::getentropy(out + offset, chunk) != 0) {
            int err = errno;
            // Fallback ke pembacaan langsung /dev/urandom jika getentropy gagal
            std::ifstream urandom("/dev/urandom", std::ios::binary);
            if (urandom.is_open()) {
                size_t remaining = len - offset;
                if (urandom.read(reinterpret_cast<char*>(out + offset), remaining)) {
                    return;
                }
            }
            throw std::runtime_error("secure_random: gagal mendapatkan entropi dari OS: " +
                                     std::string(std::strerror(err)));
        }
        offset += chunk;
    }
}

Bytes secure_random(size_t len) {
    Bytes b(len);
    if (len > 0) {
        secure_random(b.data(), len);
    }
    return b;
}

} // namespace util
