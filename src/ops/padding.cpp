#include "padding.hpp"

util::Bytes pkcs7_pad(const util::Bytes& in) {
    size_t pad_len = BLOCK_SIZE - (in.size() % BLOCK_SIZE);
    util::Bytes out = in;
    out.insert(out.end(), pad_len, static_cast<uint8_t>(pad_len));
    return out;
}

util::Bytes pkcs7_unpad(const util::Bytes& in) {
    if (in.empty()) {
        throw PaddingError("Data kosong, tidak memiliki padding");
    }
    if (in.size() % BLOCK_SIZE != 0) {
        throw PaddingError("Panjang data bukan kelipatan ukuran blok (16 byte)");
    }

    uint8_t pad_val = in.back();
    if (pad_val == 0 || pad_val > BLOCK_SIZE || pad_val > in.size()) {
        throw PaddingError("Nilai byte padding tidak valid");
    }

    for (size_t i = in.size() - pad_val; i < in.size(); ++i) {
        if (in[i] != pad_val) {
            throw PaddingError("Byte padding tidak seragam");
        }
    }

    return util::Bytes(in.begin(), in.end() - pad_val);
}
