#include "util.hpp"

#include <stdexcept>

namespace util {

namespace {

int parse_hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

std::string to_hex(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return "";
    }

    static const char hex_digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(len * 2);

    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        hex.push_back(hex_digits[(byte >> 4) & 0x0F]);
        hex.push_back(hex_digits[byte & 0x0F]);
    }

    return hex;
}

std::string to_hex(const Bytes& data) {
    return to_hex(data.data(), data.size());
}

Bytes from_hex(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("from_hex: Panjang string heksadesimal harus genap");
    }

    Bytes bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        int hi = parse_hex_nibble(hex[i]);
        if (hi < 0) {
            throw std::invalid_argument("from_hex: Karakter non-heksadesimal pada posisi " +
                                        std::to_string(i));
        }
        int lo = parse_hex_nibble(hex[i + 1]);
        if (lo < 0) {
            throw std::invalid_argument("from_hex: Karakter non-heksadesimal pada posisi " +
                                        std::to_string(i + 1));
        }

        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }

    return bytes;
}

} // namespace util
