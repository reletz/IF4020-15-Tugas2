#include "mac.hpp"
#include "cipher.hpp"
#include "util.hpp"
#include "test_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

namespace {

// Minimal mock block cipher for deterministic CMAC testing (encrypt only)
class SimpleCipher : public BlockCipher {
public:
    explicit SimpleCipher(const uint8_t key[KEY_SIZE]) {
        std::memcpy(key_, key, KEY_SIZE);
    }
    void encrypt_block(const uint8_t* in, uint8_t* out) const override {
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            out[i] = static_cast<uint8_t>(in[(i + 1) % BLOCK_SIZE] ^ key_[i] ^ (i * 17 + 1));
        }
    }
    void decrypt_block(const uint8_t*, uint8_t*) const override {} // Unused by CMAC
private:
    uint8_t key_[KEY_SIZE];
};

void test_rfc4493_dbl() {
    // RFC 4493 Section 4 test vectors
    util::Bytes l  = util::from_hex("7df76b0c1ab899b33e42f047b91b546f");
    util::Bytes k1 = util::from_hex("fbeed618357133667c85e08f7236a8de");
    util::Bytes k2 = util::from_hex("f7ddac306ae266ccf90bc11ee46d513b");

    uint8_t out[BLOCK_SIZE];
    cmac_detail::dbl(l.data(), out);
    CHECK(util::constant_time_equal(out, k1.data(), BLOCK_SIZE), "dbl(L) == K1");

    cmac_detail::dbl(out, out); // in-place test
    CHECK(util::constant_time_equal(out, k2.data(), BLOCK_SIZE), "dbl(K1) == K2 (...513b)");

    // Edge cases: all-zero, MSB=1 (0x80 -> 0x87), LSB=1 (0x01 -> 0x02)
    uint8_t edge[BLOCK_SIZE] = {0};
    cmac_detail::dbl(edge, out);
    CHECK(util::constant_time_equal(edge, out, BLOCK_SIZE), "dbl(0) == 0");

    edge[0] = 0x80;
    cmac_detail::dbl(edge, out);
    CHECK(out[15] == 0x87 && out[0] == 0, "dbl(80..0) == 0..87");

    std::memset(edge, 0, BLOCK_SIZE);
    edge[15] = 0x01;
    cmac_detail::dbl(edge, out);
    CHECK(out[15] == 0x02 && out[0] == 0, "dbl(0..1) == 0..2");

    CHECK_THROWS(cmac_detail::dbl(nullptr, out), std::invalid_argument, "dbl null check");
}

void test_chunking_and_lengths() {
    uint8_t key[KEY_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    SimpleCipher cipher(key);

    std::vector<uint8_t> data(1000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>((i * 31 + 7) & 0xff);
    }

    const size_t lengths[] = {0, 1, 15, 16, 17, 31, 32, 33, 100, 1000};
    const size_t chunks[] = {1, 7, 15, 16, 17};

    for (size_t len : lengths) {
        uint8_t expected[MAC_TAG_SIZE];
        Cmac::compute(cipher, data.data(), len, expected);

        // Verify matches compute
        CHECK(Cmac::verify(cipher, data.data(), len, expected), "verify one-shot");

        // Verify across chunk sizes
        for (size_t cs : chunks) {
            Cmac cmac(cipher);
            for (size_t off = 0; off < len; off += cs) {
                cmac.update(data.data() + off, std::min(cs, len - off));
            }
            uint8_t actual[MAC_TAG_SIZE];
            cmac.finalize(actual);
            CHECK(util::constant_time_equal(expected, actual, MAC_TAG_SIZE), "chunked match");
        }
    }
}

void test_one_bit_tamper() {
    uint8_t key[KEY_SIZE] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
                             0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21};
    SimpleCipher cipher(key);

    std::vector<uint8_t> msg(100);
    for (size_t i = 0; i < msg.size(); ++i) msg[i] = static_cast<uint8_t>(i);

    uint8_t tag[MAC_TAG_SIZE];
    Cmac::compute(cipher, msg.data(), msg.size(), tag);
    CHECK(Cmac::verify(cipher, msg.data(), msg.size(), tag), "unmodified verify");

    // Flip all 800 bits of the message
    for (size_t i = 0; i < msg.size(); ++i) {
        for (int b = 0; b < 8; ++b) {
            msg[i] ^= static_cast<uint8_t>(1 << b);
            CHECK(!Cmac::verify(cipher, msg.data(), msg.size(), tag), "tamper msg bit");
            msg[i] ^= static_cast<uint8_t>(1 << b);
        }
    }

    // Flip all 128 bits of the tag
    for (size_t i = 0; i < MAC_TAG_SIZE; ++i) {
        for (int b = 0; b < 8; ++b) {
            tag[i] ^= static_cast<uint8_t>(1 << b);
            CHECK(!Cmac::verify(cipher, msg.data(), msg.size(), tag), "tamper tag bit");
            tag[i] ^= static_cast<uint8_t>(1 << b);
        }
    }
}

void test_padding_split() {
    uint8_t key[KEY_SIZE] = {0xaa, 0xbb, 0xcc, 0xdd, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    SimpleCipher cipher(key);

    uint8_t m15[15] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t m16[16];
    std::memcpy(m16, m15, 15);
    m16[15] = 0x80;

    uint8_t tag15[MAC_TAG_SIZE], tag16[MAC_TAG_SIZE];
    Cmac::compute(cipher, m15, 15, tag15);
    Cmac::compute(cipher, m16, 16, tag16);

    CHECK(!util::constant_time_equal(tag15, tag16, MAC_TAG_SIZE), "M vs M||80 diff tag (K1 vs K2)");
}

void test_object_reuse() {
    uint8_t key[KEY_SIZE] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6};
    SimpleCipher cipher(key);
    Cmac cmac(cipher);

    const char* msgs[] = {"hello", "world 123", "", "a much longer message spanning multiple blocks"};
    for (const char* m : msgs) {
        size_t len = std::strlen(m);
        uint8_t exp[MAC_TAG_SIZE], act[MAC_TAG_SIZE];
        Cmac::compute(cipher, reinterpret_cast<const uint8_t*>(m), len, exp);

        cmac.update(reinterpret_cast<const uint8_t*>(m), len);
        cmac.finalize(act);
        CHECK(util::constant_time_equal(exp, act, MAC_TAG_SIZE), "reusable finalize matches compute");
    }
}

void test_derive_keys() {
    uint8_t master_key[KEY_SIZE] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    CustomCipher master_cipher(master_key);

    DerivedKeys dk1 = derive_keys(master_cipher);
    DerivedKeys dk2 = derive_keys(master_cipher);

    CHECK(util::constant_time_equal(dk1.enc, dk2.enc, KEY_SIZE), "derive_keys enc deterministic");
    CHECK(util::constant_time_equal(dk1.mac, dk2.mac, KEY_SIZE), "derive_keys mac deterministic");
    CHECK(!util::constant_time_equal(dk1.enc, dk1.mac, KEY_SIZE), "key separation: enc != mac");
    CHECK(!util::constant_time_equal(dk1.enc, master_key, KEY_SIZE), "enc != master");

    // Verify direct PRF formula: E(0..01) and E(0..02)
    uint8_t in_block[BLOCK_SIZE] = {0}, exp[BLOCK_SIZE];
    in_block[15] = 0x01;
    master_cipher.encrypt_block(in_block, exp);
    CHECK(util::constant_time_equal(dk1.enc, exp, KEY_SIZE), "enc == E(1)");

    in_block[15] = 0x02;
    master_cipher.encrypt_block(in_block, exp);
    CHECK(util::constant_time_equal(dk1.mac, exp, KEY_SIZE), "mac == E(2)");
}

void test_custom_cipher_smoke() {
    uint8_t key[KEY_SIZE] = {0xca, 0xfe, 0xba, 0xbe, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    CustomCipher cipher(key);

    uint8_t msg[32] = {'h', 'e', 'l', 'l', 'o'};
    uint8_t tag[MAC_TAG_SIZE];
    Cmac::compute(cipher, msg, sizeof(msg), tag);

    CHECK(Cmac::verify(cipher, msg, sizeof(msg), tag), "smoke CustomCipher verify");
    msg[0] ^= 1;
    CHECK(!Cmac::verify(cipher, msg, sizeof(msg), tag), "smoke CustomCipher tamper");
}

void test_api_safety() {
    uint8_t key[KEY_SIZE] = {0};
    SimpleCipher cipher(key);
    Cmac cmac(cipher);

    // Error handling
    CHECK_NO_THROW(cmac.update(nullptr, 0), "update null 0 no-op");
    CHECK_THROWS(cmac.update(nullptr, 5), std::invalid_argument, "update null throws");
    CHECK_THROWS(cmac.finalize(nullptr), std::invalid_argument, "finalize null throws");

    // Rvalue prevention: Cmac(const BlockCipher&&) must be deleted
    static_assert(!std::is_constructible_v<Cmac, const BlockCipher&&>, "rvalue cipher deleted");
}

} // namespace

int main() {
    std::cout << "--- Unit Test mac (CMAC & derive_keys) ---\n";
    test_rfc4493_dbl();
    test_chunking_and_lengths();
    test_one_bit_tamper();
    test_padding_split();
    test_object_reuse();
    test_derive_keys();
    test_custom_cipher_smoke();
    test_api_safety();

    return test::report();
}
