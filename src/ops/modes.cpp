#include "modes.hpp"

#include <algorithm>
#include <cstring>

namespace modes {

void ctr_at(const uint8_t iv[BLOCK_SIZE], uint64_t i, uint8_t out[BLOCK_SIZE]) {
    if (iv == nullptr || out == nullptr) {
        throw std::invalid_argument("Pointer iv atau out bernilai nullptr di ctr_at");
    }
    uint64_t carry = i;
    for (int j = static_cast<int>(BLOCK_SIZE) - 1; j >= 0; --j) {
        uint64_t sum = static_cast<uint64_t>(iv[j]) + (carry & 0xFF);
        out[j] = static_cast<uint8_t>(sum & 0xFF);
        carry = (carry >> 8) + (sum >> 8);
    }
}

util::Bytes ecb_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    (void)iv;
    if (in.size() % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Ukuran masukan ECB encrypt harus kelipatan BLOCK_SIZE (16 byte)");
    }
    util::Bytes out(in.size());
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        cipher.encrypt_block(in.data() + i, out.data() + i);
    }
    return out;
}

util::Bytes ecb_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    (void)iv;
    if (in.size() % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Ukuran masukan ECB decrypt harus kelipatan BLOCK_SIZE (16 byte)");
    }
    util::Bytes out(in.size());
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        cipher.decrypt_block(in.data() + i, out.data() + i);
    }
    return out;
}

util::Bytes cbc_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.size() % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Ukuran masukan CBC encrypt harus kelipatan BLOCK_SIZE (16 byte)");
    }
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode CBC");
    }

    util::Bytes out(in.size());
    uint8_t block[BLOCK_SIZE];
    const uint8_t* prev = iv;

    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        util::xor_block(in.data() + i, prev, block, BLOCK_SIZE);
        cipher.encrypt_block(block, out.data() + i);
        prev = out.data() + i;
    }
    return out;
}

util::Bytes cbc_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.size() % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Ukuran masukan CBC decrypt harus kelipatan BLOCK_SIZE (16 byte)");
    }
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode CBC");
    }

    util::Bytes out(in.size());
    uint8_t block[BLOCK_SIZE];
    const uint8_t* prev = iv;

    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        cipher.decrypt_block(in.data() + i, block);
        util::xor_block(block, prev, out.data() + i, BLOCK_SIZE);
        prev = in.data() + i;
    }
    return out;
}

util::Bytes cfb_encrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode CFB");
    }

    util::Bytes out(in.size());
    uint8_t fb[BLOCK_SIZE], ks[BLOCK_SIZE];
    std::memcpy(fb, iv, BLOCK_SIZE);

    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        cipher.encrypt_block(fb, ks);
        util::xor_block(in.data() + i, ks, out.data() + i, n);
        std::memcpy(fb, out.data() + i, n);
    }
    return out;
}

util::Bytes cfb_decrypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode CFB");
    }

    util::Bytes out(in.size());
    uint8_t fb[BLOCK_SIZE], ks[BLOCK_SIZE];
    std::memcpy(fb, iv, BLOCK_SIZE);

    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        cipher.encrypt_block(fb, ks);
        util::xor_block(in.data() + i, ks, out.data() + i, n);
        std::memcpy(fb, in.data() + i, n);
    }
    return out;
}

util::Bytes ofb_crypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode OFB");
    }

    util::Bytes out(in.size());
    uint8_t st[BLOCK_SIZE];
    std::memcpy(st, iv, BLOCK_SIZE);

    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        cipher.encrypt_block(st, st);
        util::xor_block(in.data() + i, st, out.data() + i, n);
    }
    return out;
}

util::Bytes ctr_crypt(const BlockCipher& cipher, const uint8_t* iv, const util::Bytes& in) {
    if (in.empty()) {
        return {};
    }
    if (iv == nullptr) {
        throw std::invalid_argument("IV tidak boleh bernilai nullptr pada mode CTR");
    }

    util::Bytes out(in.size());
    uint8_t ctr[BLOCK_SIZE], ks[BLOCK_SIZE];
    size_t num_blocks = (in.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (size_t b = 0; b < num_blocks; ++b) {
        size_t offset = b * BLOCK_SIZE;
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - offset);
        ctr_at(iv, static_cast<uint64_t>(b), ctr);
        cipher.encrypt_block(ctr, ks);
        util::xor_block(in.data() + offset, ks, out.data() + offset, n);
    }
    return out;
}

}
