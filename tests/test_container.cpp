#include "container.hpp"
#include "test_helpers.hpp"
#include <cstring>

static void test_roundtrip_all_modes() {
    uint8_t key[KEY_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    SecureEnvelope env(key);
    std::vector<size_t> lengths = {0, 1, 15, 16, 17, 1000};
    Mode modes[] = {Mode::ECB, Mode::CBC, Mode::CFB, Mode::OFB, Mode::CTR};

    for (Mode mode : modes) {
        for (size_t len : lengths) {
            util::Bytes pt(len);
            for (size_t i = 0; i < len; ++i) pt[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);

            util::Bytes blob1 = env.seal(pt, mode);
            CHECK(blob1.size() >= CONTAINER_MIN_SIZE, "Blob size must be >= 38");
            CHECK(blob1[0] == 'I' && blob1[1] == 'F' && blob1[2] == '1' && blob1[3] == '5', "Magic match");
            CHECK(blob1[4] == 0x01, "Version match");
            CHECK(blob1[5] == static_cast<uint8_t>(mode), "Mode ID match");

            bool all_zero_iv = true;
            for (size_t i = 6; i < 22; ++i) {
                if (blob1[i] != 0) all_zero_iv = false;
            }
            if (mode == Mode::ECB) {
                CHECK(all_zero_iv, "ECB IV must be all-zero");
            } else {
                util::Bytes blob2 = env.seal(pt, mode);
                CHECK(blob1 != blob2, "Non-ECB seals must produce different blobs");
                bool diff_iv = std::memcmp(blob1.data() + 6, blob2.data() + 6, 16) != 0;
                CHECK(diff_iv, "Non-ECB seals must have different random IVs");
            }

            util::Bytes dec = env.open(blob1);
            CHECK(dec == pt, "Decrypted plaintext must match original for mode " + mode_name(mode));
        }
    }
}

static void test_bit_tamper() {
    uint8_t key[KEY_SIZE] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00};
    SecureEnvelope env(key);
    util::Bytes pt = {'S', 'e', 'c', 'u', 'r', 'e', 'C', 'o', 'n', 't', 'a', 'i', 'n', 'e', 'r', '!'};
    util::Bytes blob = env.seal(pt, Mode::CBC);

    size_t tamper_count = 0;
    for (size_t i = 0; i < blob.size(); ++i) {
        for (int b = 0; b < 8; ++b) {
            util::Bytes tampered = blob;
            tampered[i] ^= static_cast<uint8_t>(1 << b);
            bool caught = false;
            try {
                env.open(tampered);
            } catch (const FormatError&) {
                caught = true;
            } catch (const AuthenticationError&) {
                caught = true;
            }
            CHECK(caught, "Bit tamper at byte " + std::to_string(i) + " bit " + std::to_string(b) + " must throw");
            tamper_count++;
        }
    }
    CHECK(tamper_count == blob.size() * 8, "All bits tampered and verified");
}

static void test_truncated_and_invalid_blobs() {
    uint8_t key[KEY_SIZE] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    SecureEnvelope env(key);
    util::Bytes pt(16, 0x42);
    util::Bytes blob = env.seal(pt, Mode::CTR);

    CHECK_THROWS(env.open(util::Bytes()), FormatError, "0-byte blob throws FormatError");
    CHECK_THROWS(env.open(util::Bytes(1, 0x49)), FormatError, "1-byte blob throws FormatError");
    CHECK_THROWS(env.open(util::Bytes(blob.begin(), blob.begin() + 37)), FormatError, "37-byte blob throws FormatError");

    util::Bytes truncated_ct = blob;
    truncated_ct.pop_back();
    CHECK_THROWS(env.open(truncated_ct), AuthenticationError, "Cut last byte throws AuthenticationError");

    util::Bytes cbc_blob = env.seal(pt, Mode::CBC);
    cbc_blob[5] = static_cast<uint8_t>(Mode::CTR);
    CHECK_THROWS(env.open(cbc_blob), AuthenticationError, "Tampered mode byte must throw AuthenticationError");
}

static void test_wrong_key() {
    uint8_t key1[KEY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t key2[KEY_SIZE] = {0x11, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    SecureEnvelope env1(key1);
    SecureEnvelope env2(key2);

    util::Bytes pt = {'S', 'e', 'c', 'r', 'e', 't'};
    util::Bytes blob = env1.seal(pt, Mode::CTR);
    CHECK_THROWS(env2.open(blob), AuthenticationError, "Wrong key must throw AuthenticationError");
}

static void test_mode_helpers() {
    CHECK(parse_mode("ecb") == Mode::ECB, "parse_mode ecb");
    CHECK(parse_mode("CBC") == Mode::CBC, "parse_mode CBC uppercase");
    CHECK(parse_mode("Cfb") == Mode::CFB, "parse_mode Cfb mixed");
    CHECK(parse_mode("ofb") == Mode::OFB, "parse_mode ofb");
    CHECK(parse_mode("CTR") == Mode::CTR, "parse_mode CTR");
    CHECK_THROWS(parse_mode("gcm"), std::invalid_argument, "parse_mode invalid throws");

    CHECK(mode_name(Mode::ECB) == "ecb", "mode_name ECB");
    CHECK(mode_name(Mode::CBC) == "cbc", "mode_name CBC");
    CHECK(mode_name(Mode::CFB) == "cfb", "mode_name CFB");
    CHECK(mode_name(Mode::OFB) == "ofb", "mode_name OFB");
    CHECK(mode_name(Mode::CTR) == "ctr", "mode_name CTR");
}

static void test_custom_iv() {
    uint8_t key[KEY_SIZE] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    uint8_t custom_iv[BLOCK_SIZE] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    SecureEnvelope env(key);
    util::Bytes pt = {'C', 'u', 's', 't', 'o', 'm', 'I', 'V', 'T', 'e', 's', 't'};

    util::Bytes blob = env.seal(pt, Mode::CBC, custom_iv);
    CHECK(std::memcmp(blob.data() + 6, custom_iv, BLOCK_SIZE) == 0, "Custom IV must match in container header");
    util::Bytes dec = env.open(blob);
    CHECK(dec == pt, "Decryption with custom IV matches plaintext");
}

int main() {
    std::cout << "--- Unit Test container (SecureEnvelope) ---\n";
    test_mode_helpers();
    test_roundtrip_all_modes();
    test_custom_iv();
    test_bit_tamper();
    test_truncated_and_invalid_blobs();
    test_wrong_key();
    return test::report();
}
