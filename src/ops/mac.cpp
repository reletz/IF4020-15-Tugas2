#include "mac.hpp"
#include "util.hpp"
#include <cstring>
#include <stdexcept>

namespace cmac_detail {

void dbl(const uint8_t in[16], uint8_t out[16]) {
    if (in == nullptr || out == nullptr) {
        throw std::invalid_argument("cmac_detail::dbl: pointer null");
    }

    uint8_t msb = static_cast<uint8_t>(in[0] >> 7);
    uint8_t mask = static_cast<uint8_t>(0 - msb);

    uint8_t tmp[16];
    for (size_t i = 0; i < 15; ++i) {
        tmp[i] = static_cast<uint8_t>((in[i] << 1) | (in[i + 1] >> 7));
    }
    tmp[15] = static_cast<uint8_t>((in[15] << 1) ^ (0x87 & mask));

    std::memcpy(out, tmp, sizeof(tmp));
    util::secure_zero(tmp, sizeof(tmp));
}

} // namespace cmac_detail

Cmac::Cmac(const BlockCipher& cipher) : cipher_(cipher), buf_len_(0) {
    uint8_t zero[BLOCK_SIZE] = {0}, l[BLOCK_SIZE] = {0};
    cipher_.encrypt_block(zero, l);

    cmac_detail::dbl(l, k1_);
    cmac_detail::dbl(k1_, k2_);

    util::secure_zero(l, sizeof(l));
    reset_state();
}

Cmac::~Cmac() {
    util::secure_zero(k1_, sizeof(k1_));
    util::secure_zero(k2_, sizeof(k2_));
    reset_state();
}

void Cmac::reset_state() {
    util::secure_zero(x_, sizeof(x_));
    util::secure_zero(buf_, sizeof(buf_));
    buf_len_ = 0;
}

void Cmac::process_buf() {
    uint8_t block[BLOCK_SIZE];
    util::xor_block(x_, buf_, block, BLOCK_SIZE);
    cipher_.encrypt_block(block, x_);
    util::secure_zero(block, sizeof(block));
    buf_len_ = 0;
}

void Cmac::update(const uint8_t* data, size_t len) {
    if (len == 0) return;
    if (data == nullptr) {
        throw std::invalid_argument("Cmac::update: pointer data null pada panjang > 0");
    }

    // 1. Jika buffer penuh dan data baru tiba, isi buffer bukan blok terakhir -> proses ke X
    if (buf_len_ == BLOCK_SIZE) {
        process_buf();
    }

    // 2. Jika buffer terisi sebagian, isi sampai penuh
    if (buf_len_ > 0) {
        size_t needed = BLOCK_SIZE - buf_len_;
        if (len <= needed) {
            std::memcpy(buf_ + buf_len_, data, len);
            buf_len_ += len;
            return;
        }
        std::memcpy(buf_ + buf_len_, data, needed);
        data += needed;
        len -= needed;
        process_buf();
    }

    // 3. Proses blok-blok penuh langsung dari data, sisakan 1..BLOCK_SIZE byte untuk buffer
    while (len > BLOCK_SIZE) {
        uint8_t block[BLOCK_SIZE];
        util::xor_block(x_, data, block, BLOCK_SIZE);
        cipher_.encrypt_block(block, x_);
        util::secure_zero(block, sizeof(block));
        data += BLOCK_SIZE;
        len -= BLOCK_SIZE;
    }

    // 4. Tahan sisa 1..16 byte di buffer (blok terakhir butuh penanganan K1/K2 di finalize)
    std::memcpy(buf_, data, len);
    buf_len_ = len;
}

void Cmac::finalize(uint8_t tag[MAC_TAG_SIZE]) {
    if (tag == nullptr) {
        throw std::invalid_argument("Cmac::finalize: pointer tag null");
    }

    uint8_t last_block[BLOCK_SIZE];

    if (buf_len_ == BLOCK_SIZE) {
        // Blok lengkap: XOR dengan K1
        util::xor_block(buf_, k1_, last_block, BLOCK_SIZE);
    } else {
        // Blok tidak lengkap / pesan kosong: pad 0x80 lalu 0x00..., XOR dengan K2
        uint8_t padded[BLOCK_SIZE] = {0};
        if (buf_len_ > 0) {
            std::memcpy(padded, buf_, buf_len_);
        }
        padded[buf_len_] = 0x80;
        util::xor_block(padded, k2_, last_block, BLOCK_SIZE);
        util::secure_zero(padded, sizeof(padded));
    }

    // Y = X xor last_block, Tag = E(Y)
    uint8_t y[BLOCK_SIZE];
    util::xor_block(x_, last_block, y, BLOCK_SIZE);
    cipher_.encrypt_block(y, tag);

    util::secure_zero(last_block, sizeof(last_block));
    util::secure_zero(y, sizeof(y));
    reset_state();
}

void Cmac::compute(const BlockCipher& cipher, const uint8_t* data, size_t len, uint8_t tag[MAC_TAG_SIZE]) {
    Cmac cmac(cipher);
    cmac.update(data, len);
    cmac.finalize(tag);
}

bool Cmac::verify(const BlockCipher& cipher, const uint8_t* data, size_t len, const uint8_t tag[MAC_TAG_SIZE]) {
    if (tag == nullptr) {
        throw std::invalid_argument("Cmac::verify: pointer tag null");
    }
    uint8_t expected[MAC_TAG_SIZE];
    compute(cipher, data, len, expected);
    bool ok = util::constant_time_equal(expected, tag, MAC_TAG_SIZE);
    util::secure_zero(expected, sizeof(expected));
    return ok;
}

DerivedKeys derive_keys(const BlockCipher& master_cipher) {
    static_assert(KEY_SIZE == BLOCK_SIZE, "KEY_SIZE harus sama dengan BLOCK_SIZE");
    DerivedKeys keys;
    uint8_t block[BLOCK_SIZE] = {0};

    // enc = E_master(00 00 ... 00 01)
    block[BLOCK_SIZE - 1] = 0x01;
    master_cipher.encrypt_block(block, keys.enc);

    // mac = E_master(00 00 ... 00 02)
    block[BLOCK_SIZE - 1] = 0x02;
    master_cipher.encrypt_block(block, keys.mac);

    util::secure_zero(block, sizeof(block));
    return keys;
}
