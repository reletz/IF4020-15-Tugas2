#include "modes.hpp"
#include "cipher.hpp"
#include "test_helpers.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {

const uint8_t TEST_KEY[KEY_SIZE] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

const uint8_t TEST_IV[BLOCK_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

util::Bytes make_pattern(size_t len) {
    util::Bytes out(len);
    for (size_t i = 0; i < len; ++i) {
        out[i] = static_cast<uint8_t>((i * 47 + 7) & 0xFF);
    }
    return out;
}

}

void test_ctr_at_helper() {
    uint8_t iv[BLOCK_SIZE] = {0};
    uint8_t out[BLOCK_SIZE] = {0};

    // 1. iv + 0 == iv
    modes::ctr_at(iv, 0, out);
    CHECK(std::memcmp(iv, out, BLOCK_SIZE) == 0, "ctr_at + 0 menghasilkan nilai identik");

    // 2. Increment biasa
    modes::ctr_at(iv, 5, out);
    CHECK(out[15] == 5, "ctr_at + 5 pada iv nol");

    // 3. Increment dengan carry pada byte rendah (00..00 ff ff + 1 -> 00..01 00 00)
    uint8_t iv_carry[BLOCK_SIZE] = {0};
    iv_carry[14] = 0xFF;
    iv_carry[15] = 0xFF;
    modes::ctr_at(iv_carry, 1, out);
    CHECK(out[13] == 0x01 && out[14] == 0x00 && out[15] == 0x00, "ctr_at carry 2 byte beruntun");

    // 4. Carry beruntun banyak blok: iv_carry + 2
    modes::ctr_at(iv_carry, 2, out);
    CHECK(out[13] == 0x01 && out[14] == 0x00 && out[15] == 0x01, "ctr_at iv_carry + 2");

    // 5. Wrap around penuh 128-bit: ff..ff + 1 -> 00..00
    uint8_t iv_max[BLOCK_SIZE];
    std::memset(iv_max, 0xFF, BLOCK_SIZE);
    modes::ctr_at(iv_max, 1, out);
    uint8_t zero_block[BLOCK_SIZE] = {0};
    CHECK(std::memcmp(out, zero_block, BLOCK_SIZE) == 0, "ctr_at wrap around penuh 128 bit");

    // 6. Wrap around ff..ff + 5 -> 00..04
    modes::ctr_at(iv_max, 5, out);
    CHECK(out[15] == 0x04 && out[0] == 0x00, "ctr_at wrap around + 5");

    // 7. Null pointer exception
    CHECK_THROWS(modes::ctr_at(nullptr, 1, out), std::invalid_argument, "ctr_at reject null iv");
    CHECK_THROWS(modes::ctr_at(iv, 1, nullptr), std::invalid_argument, "ctr_at reject null out");
}

void test_ecb_cbc_roundtrip() {
    CustomCipher cipher(TEST_KEY);
    const size_t test_lens[] = {0, 16, 32, 1600};

    for (size_t len : test_lens) {
        util::Bytes pt = make_pattern(len);

        // ECB
        util::Bytes ct_ecb = modes::ecb_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_ecb.size() == len, "ECB ukuran cocok len " + std::to_string(len));
        util::Bytes dec_ecb = modes::ecb_decrypt(cipher, TEST_IV, ct_ecb);
        CHECK(dec_ecb == pt, "ECB round-trip cocok len " + std::to_string(len));

        // CBC
        util::Bytes ct_cbc = modes::cbc_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_cbc.size() == len, "CBC ukuran cocok len " + std::to_string(len));
        util::Bytes dec_cbc = modes::cbc_decrypt(cipher, TEST_IV, ct_cbc);
        CHECK(dec_cbc == pt, "CBC round-trip cocok len " + std::to_string(len));
    }
}

void test_stream_modes_roundtrip() {
    CustomCipher cipher(TEST_KEY);
    const size_t test_lens[] = {0, 1, 15, 16, 17, 1000, 4096 + 7};

    for (size_t len : test_lens) {
        util::Bytes pt = make_pattern(len);

        // 1. CFB
        util::Bytes ct_cfb = modes::cfb_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_cfb.size() == len, "CFB panjang ciphertext == plaintext len " + std::to_string(len));
        util::Bytes dec_cfb = modes::cfb_decrypt(cipher, TEST_IV, ct_cfb);
        CHECK(dec_cfb == pt, "CFB round-trip cocok len " + std::to_string(len));

        // 2. OFB
        util::Bytes ct_ofb = modes::ofb_crypt(cipher, TEST_IV, pt);
        CHECK(ct_ofb.size() == len, "OFB panjang ciphertext == plaintext len " + std::to_string(len));
        util::Bytes dec_ofb = modes::ofb_crypt(cipher, TEST_IV, ct_ofb);
        CHECK(dec_ofb == pt, "OFB round-trip cocok len " + std::to_string(len));

        // 3. CTR
        util::Bytes ct_ctr = modes::ctr_crypt(cipher, TEST_IV, pt);
        CHECK(ct_ctr.size() == len, "CTR panjang ciphertext == plaintext len " + std::to_string(len));
        util::Bytes dec_ctr = modes::ctr_crypt(cipher, TEST_IV, ct_ctr);
        CHECK(dec_ctr == pt, "CTR round-trip cocok len " + std::to_string(len));
    }
}

void test_invalid_length_and_args() {
    CustomCipher cipher(TEST_KEY);

    const size_t invalid_lens[] = {1, 15, 17, 31, 33};
    for (size_t len : invalid_lens) {
        util::Bytes pt = make_pattern(len);
        CHECK_THROWS(modes::ecb_encrypt(cipher, TEST_IV, pt), std::invalid_argument,
                     "ECB encrypt tolak non-kelipatan 16");
        CHECK_THROWS(modes::ecb_decrypt(cipher, TEST_IV, pt), std::invalid_argument,
                     "ECB decrypt tolak non-kelipatan 16");
        CHECK_THROWS(modes::cbc_encrypt(cipher, TEST_IV, pt), std::invalid_argument,
                     "CBC encrypt tolak non-kelipatan 16");
        CHECK_THROWS(modes::cbc_decrypt(cipher, TEST_IV, pt), std::invalid_argument,
                     "CBC decrypt tolak non-kelipatan 16");
    }

    // Nullptr IV pada data non-empty
    util::Bytes pt16 = make_pattern(16);
    CHECK_THROWS(modes::cbc_encrypt(cipher, nullptr, pt16), std::invalid_argument, "CBC enc tolak null IV");
    CHECK_THROWS(modes::cbc_decrypt(cipher, nullptr, pt16), std::invalid_argument, "CBC dec tolak null IV");
    CHECK_THROWS(modes::cfb_encrypt(cipher, nullptr, pt16), std::invalid_argument, "CFB enc tolak null IV");
    CHECK_THROWS(modes::cfb_decrypt(cipher, nullptr, pt16), std::invalid_argument, "CFB dec tolak null IV");
    CHECK_THROWS(modes::ofb_crypt(cipher, nullptr, pt16), std::invalid_argument, "OFB crypt tolak null IV");
    CHECK_THROWS(modes::ctr_crypt(cipher, nullptr, pt16), std::invalid_argument, "CTR crypt tolak null IV");
}

void test_relational_properties() {
    CustomCipher cipher(TEST_KEY);

    // Dua blok plaintext identik
    util::Bytes block16(16, 0x42);
    util::Bytes double_block = block16;
    double_block.insert(double_block.end(), block16.begin(), block16.end());

    // 1. ECB: dua blok identik -> ciphertext identik
    util::Bytes ct_ecb = modes::ecb_encrypt(cipher, nullptr, double_block);
    CHECK(ct_ecb.size() == 32, "ECB 2 blok output 32 byte");
    bool ecb_blocks_identical = (std::memcmp(ct_ecb.data(), ct_ecb.data() + 16, 16) == 0);
    CHECK(ecb_blocks_identical, "ECB: dua blok identik menghasilkan ciphertext identik (kelemahan ECB)");

    // 2. CBC: dua blok identik -> ciphertext berbeda
    util::Bytes ct_cbc = modes::cbc_encrypt(cipher, TEST_IV, double_block);
    CHECK(ct_cbc.size() == 32, "CBC 2 blok output 32 byte");
    bool cbc_blocks_different = (std::memcmp(ct_cbc.data(), ct_cbc.data() + 16, 16) != 0);
    CHECK(cbc_blocks_different, "CBC: dua blok identik menghasilkan ciphertext berbeda");

    // 3. CBC IV=0: blok pertama CBC == blok pertama ECB
    uint8_t zero_iv[BLOCK_SIZE] = {0};
    util::Bytes ct_cbc_zero_iv = modes::cbc_encrypt(cipher, zero_iv, double_block);
    bool first_block_match = (std::memcmp(ct_ecb.data(), ct_cbc_zero_iv.data(), 16) == 0);
    CHECK(first_block_match, "CBC dengan IV=0 menghasilkan blok pertama identik dengan ECB");

    // 4. CTR: blok 0 == P_0 ^ E(IV), blok 1 == P_1 ^ E(IV+1)
    util::Bytes ct_ctr = modes::ctr_crypt(cipher, TEST_IV, double_block);
    uint8_t ks0[BLOCK_SIZE], ks1[BLOCK_SIZE], ctr1[BLOCK_SIZE];
    cipher.encrypt_block(TEST_IV, ks0);
    modes::ctr_at(TEST_IV, 1, ctr1);
    cipher.encrypt_block(ctr1, ks1);

    uint8_t expected_c0[BLOCK_SIZE], expected_c1[BLOCK_SIZE];
    util::xor_block(double_block.data(), ks0, expected_c0, BLOCK_SIZE);
    util::xor_block(double_block.data() + 16, ks1, expected_c1, BLOCK_SIZE);
    CHECK(std::memcmp(ct_ctr.data(), expected_c0, BLOCK_SIZE) == 0, "CTR blok 0 == P_0 ^ E(IV)");
    CHECK(std::memcmp(ct_ctr.data() + 16, expected_c1, BLOCK_SIZE) == 0, "CTR blok 1 == P_1 ^ E(IV+1)");

    // 5. OFB: keystream == E(IV), E(E(IV))
    util::Bytes ct_ofb = modes::ofb_crypt(cipher, TEST_IV, double_block);
    uint8_t ofb_st1[BLOCK_SIZE], ofb_st2[BLOCK_SIZE];
    cipher.encrypt_block(TEST_IV, ofb_st1);
    cipher.encrypt_block(ofb_st1, ofb_st2);
    uint8_t exp_ofb0[BLOCK_SIZE], exp_ofb1[BLOCK_SIZE];
    util::xor_block(double_block.data(), ofb_st1, exp_ofb0, BLOCK_SIZE);
    util::xor_block(double_block.data() + 16, ofb_st2, exp_ofb1, BLOCK_SIZE);
    CHECK(std::memcmp(ct_ofb.data(), exp_ofb0, BLOCK_SIZE) == 0, "OFB blok 0 == P_0 ^ E(IV)");
    CHECK(std::memcmp(ct_ofb.data() + 16, exp_ofb1, BLOCK_SIZE) == 0, "OFB blok 1 == P_1 ^ E(E(IV))");

    // 6. CFB: blok 1 == P_1 ^ E(C_0)
    util::Bytes ct_cfb = modes::cfb_encrypt(cipher, TEST_IV, double_block);
    uint8_t cfb_ks1[BLOCK_SIZE], exp_cfb1[BLOCK_SIZE];
    cipher.encrypt_block(ct_cfb.data(), cfb_ks1);
    util::xor_block(double_block.data() + 16, cfb_ks1, exp_cfb1, BLOCK_SIZE);
    CHECK(std::memcmp(ct_cfb.data() + 16, exp_cfb1, BLOCK_SIZE) == 0, "CFB blok 1 == P_1 ^ E(C_0)");

    // 7. IV berbeda menghasilkan ciphertext berbeda
    uint8_t alt_iv[BLOCK_SIZE];
    std::memset(alt_iv, 0xEE, BLOCK_SIZE);
    CHECK(modes::cbc_encrypt(cipher, alt_iv, double_block) != ct_cbc, "CBC IV beda -> CT beda");
    CHECK(modes::cfb_encrypt(cipher, alt_iv, double_block) != ct_cfb, "CFB IV beda -> CT beda");
    CHECK(modes::ofb_crypt(cipher, alt_iv, double_block) != ct_ofb, "OFB IV beda -> CT beda");
    CHECK(modes::ctr_crypt(cipher, alt_iv, double_block) != ct_ctr, "CTR IV beda -> CT beda");

    // 8. Determinisme
    CHECK(modes::cbc_encrypt(cipher, TEST_IV, double_block) == ct_cbc, "CBC deterministik");
    CHECK(modes::cfb_encrypt(cipher, TEST_IV, double_block) == ct_cfb, "CFB deterministik");
    CHECK(modes::ofb_crypt(cipher, TEST_IV, double_block) == ct_ofb, "OFB deterministik");
    CHECK(modes::ctr_crypt(cipher, TEST_IV, double_block) == ct_ctr, "CTR deterministik");
}

void test_parallel_threshold_and_roundtrip() {
    CustomCipher cipher(TEST_KEY);

    const size_t threshold_lens[] = {
        modes::PARALLEL_THRESHOLD_BLOCKS * BLOCK_SIZE,
        (modes::PARALLEL_THRESHOLD_BLOCKS + 512) * BLOCK_SIZE,
        (modes::PARALLEL_THRESHOLD_BLOCKS * 2) * BLOCK_SIZE 
    };

    for (size_t len : threshold_lens) {
        util::Bytes pt = make_pattern(len);

        // 1. ECB
        util::Bytes ct_ecb = modes::ecb_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_ecb.size() == len, "ECB ukuran threshold cocok len " + std::to_string(len));
        util::Bytes dec_ecb = modes::ecb_decrypt(cipher, TEST_IV, ct_ecb);
        CHECK(dec_ecb == pt, "ECB round-trip di atas threshold cocok len " + std::to_string(len));

        // 2. CBC (Enkripsi serial, Dekripsi paralel)
        util::Bytes ct_cbc = modes::cbc_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_cbc.size() == len, "CBC ukuran threshold cocok len " + std::to_string(len));
        util::Bytes dec_cbc = modes::cbc_decrypt(cipher, TEST_IV, ct_cbc);
        CHECK(dec_cbc == pt, "CBC round-trip di atas threshold cocok len " + std::to_string(len));

        // 3. CFB (Enkripsi serial, Dekripsi paralel)
        util::Bytes ct_cfb = modes::cfb_encrypt(cipher, TEST_IV, pt);
        CHECK(ct_cfb.size() == len, "CFB ukuran threshold cocok len " + std::to_string(len));
        util::Bytes dec_cfb = modes::cfb_decrypt(cipher, TEST_IV, ct_cfb);
        CHECK(dec_cfb == pt, "CFB round-trip di atas threshold cocok len " + std::to_string(len));

        // 4. OFB (Serial)
        util::Bytes ct_ofb = modes::ofb_crypt(cipher, TEST_IV, pt);
        CHECK(ct_ofb.size() == len, "OFB ukuran threshold cocok len " + std::to_string(len));
        util::Bytes dec_ofb = modes::ofb_crypt(cipher, TEST_IV, ct_ofb);
        CHECK(dec_ofb == pt, "OFB round-trip di atas threshold cocok len " + std::to_string(len));

        // 5. CTR (Paralel penuh)
        util::Bytes ct_ctr = modes::ctr_crypt(cipher, TEST_IV, pt);
        CHECK(ct_ctr.size() == len, "CTR ukuran threshold cocok len " + std::to_string(len));
        util::Bytes dec_ctr = modes::ctr_crypt(cipher, TEST_IV, ct_ctr);
        CHECK(dec_ctr == pt, "CTR round-trip di atas threshold cocok len " + std::to_string(len));

        size_t sample_b = (len / BLOCK_SIZE) - 1;
        size_t sample_offset = sample_b * BLOCK_SIZE;
        uint8_t sample_block_pt[BLOCK_SIZE], sample_block_ct[BLOCK_SIZE];
        std::memcpy(sample_block_pt, pt.data() + sample_offset, BLOCK_SIZE);
        cipher.encrypt_block(sample_block_pt, sample_block_ct);
        CHECK(std::memcmp(ct_ecb.data() + sample_offset, sample_block_ct, BLOCK_SIZE) == 0,
              "ECB blok sampel paralel identik dengan enkripsi blok serial acuan");

        uint8_t sample_ctr[BLOCK_SIZE], sample_ks[BLOCK_SIZE], exp_ctr_ct[BLOCK_SIZE];
        modes::ctr_at(TEST_IV, sample_b, sample_ctr);
        cipher.encrypt_block(sample_ctr, sample_ks);
        util::xor_block(sample_block_pt, sample_ks, exp_ctr_ct, BLOCK_SIZE);
        CHECK(std::memcmp(ct_ctr.data() + sample_offset, exp_ctr_ct, BLOCK_SIZE) == 0,
              "CTR blok sampel paralel identik dengan enkripsi blok serial acuan");
    }

    size_t non_multiple_len = (modes::PARALLEL_THRESHOLD_BLOCKS + 100) * BLOCK_SIZE + 13;
    util::Bytes pt_partial = make_pattern(non_multiple_len);
    util::Bytes ct_partial = modes::ctr_crypt(cipher, TEST_IV, pt_partial);
    CHECK(ct_partial.size() == non_multiple_len, "CTR ukuran parsial di atas threshold cocok");
    util::Bytes dec_partial = modes::ctr_crypt(cipher, TEST_IV, ct_partial);
    CHECK(dec_partial == pt_partial, "CTR round-trip ukuran parsial di atas threshold cocok");
}

int main() {
    test_ctr_at_helper();
    test_ecb_cbc_roundtrip();
    test_stream_modes_roundtrip();
    test_invalid_length_and_args();
    test_relational_properties();
    test_parallel_threshold_and_roundtrip();
    return test::report();
}
