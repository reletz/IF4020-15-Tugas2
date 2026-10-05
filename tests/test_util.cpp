#include "util.hpp"
#include "test_helpers.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <vector>

void test_hex() {
    // 1. Round-trip empty & arbitrary bytes & all 256 values
    CHECK(util::to_hex(util::Bytes{}).empty(), "to_hex empty");
    CHECK(util::from_hex("").empty(), "from_hex empty");

    util::Bytes data = {0x00, 0x01, 0x0a, 0x0f, 0x10, 0x7f, 0x80, 0xfe, 0xff};
    CHECK(util::to_hex(data) == "00010a0f107f80feff", "to_hex lowercase");
    CHECK(util::from_hex("00010a0f107f80feff") == data, "from_hex round-trip");

    util::Bytes all_bytes(256);
    for (size_t i = 0; i < 256; ++i) all_bytes[i] = static_cast<uint8_t>(i);
    CHECK(util::from_hex(util::to_hex(all_bytes)) == all_bytes, "256-byte round-trip");

    // 2. Reject odd lengths and invalid characters
    CHECK_THROWS(util::from_hex("abc"), std::invalid_argument, "reject odd length");
    CHECK_THROWS(util::from_hex("0g"), std::invalid_argument, "reject non-hex");
    CHECK_THROWS(util::from_hex("1 "), std::invalid_argument, "reject spaces");

    // Error message must not leak input content
    try {
        util::from_hex("00zz");
        CHECK(false, "from_hex 00zz must throw");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("zz") == std::string::npos, "no input leak in exception");
    }

    // 3. Case insensitivity
    util::Bytes expected = {0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f};
    CHECK(util::from_hex("0A1B2C3D4E5F") == expected, "accept uppercase");
    CHECK(util::from_hex("0a1B2c3D4e5F") == expected, "accept mixed case");
}

void test_secure_random() {
    util::Bytes r1 = util::secure_random(16);
    util::Bytes r2 = util::secure_random(16);
    CHECK(r1.size() == 16 && r2.size() == 16 && r1 != r2, "secure_random produces unique 16 bytes");
    CHECK(util::secure_random(0).empty(), "secure_random(0) empty");
    CHECK(util::secure_random(600).size() == 600, "secure_random spans across chunks");
    CHECK_THROWS(util::secure_random(nullptr, 16), std::invalid_argument, "null buffer throws");
}

void test_file_io() {
    test::TempDir temp_dir;
    const std::string p0 = temp_dir.path() + "/0.bin";
    const std::string p1 = temp_dir.path() + "/1.bin";
    const std::string p_all = temp_dir.path() + "/all.bin";
    const std::string p_1mb = temp_dir.path() + "/1mb.bin";

    // 1. 0-byte, 1-byte, all-byte, and 1-MiB round-trips
    util::write_file(p0, util::Bytes{});
    CHECK(util::read_file(p0).empty(), "0-byte file round-trip");

    util::Bytes one = {0x7e};
    util::write_file(p1, one);
    CHECK(util::read_file(p1) == one, "1-byte file round-trip");

    util::Bytes all(256);
    for (size_t i = 0; i < 256; ++i) all[i] = static_cast<uint8_t>(i);
    util::write_file(p_all, all);
    CHECK(util::read_file(p_all) == all, "256-byte file round-trip");

    util::Bytes large = util::secure_random(1024 * 1024);
    util::write_file(p_1mb, large);
    CHECK(util::read_file(p_1mb) == large, "1-MiB file round-trip");

    // 2. Non-existent file throws with path in message
    const std::string missing = temp_dir.path() + "/missing.bin";
    try {
        util::read_file(missing);
        CHECK(false, "read missing file must throw");
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find(missing) != std::string::npos, "exception includes path");
    }

    // 3. write_file to non-existent dir throws and leaves no leftover files
    const std::string bad_dir_file = temp_dir.path() + "/no_such_dir/out.bin";
    CHECK_THROWS(util::write_file(bad_dir_file, {1, 2}), std::runtime_error, "write bad dir throws");
    struct stat st;
    CHECK(::stat(bad_dir_file.c_str(), &st) != 0, "no bad target file created");
    CHECK(::stat((bad_dir_file + ".tmp").c_str(), &st) != 0, "no leftover tmp file");
}

void test_bytes_operations() {
    // 1. constant_time_equal
    uint8_t a[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t b[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    CHECK(util::constant_time_equal(a, b, 16), "equal buffers return true");

    b[15] = 99; CHECK(!util::constant_time_equal(a, b, 16), "diff last byte");
    b[15] = 16; b[0] = 99; CHECK(!util::constant_time_equal(a, b, 16), "diff first byte");
    b[0] = 1; b[7] = 99; CHECK(!util::constant_time_equal(a, b, 16), "diff middle byte");
    CHECK(util::constant_time_equal(nullptr, nullptr, 0), "zero length returns true");

    // 2. xor_block
    uint8_t x[4] = {0x0f, 0xf0, 0xaa, 0x55}, y[4] = {0xff, 0x0f, 0x55, 0xaa}, out[4];
    util::xor_block(x, y, out, 4);
    CHECK(out[0] == 0xf0 && out[1] == 0xff && out[2] == 0xff && out[3] == 0xff, "xor_block correct");

    util::xor_block(x, y, x, 4); // in-place
    CHECK(x[0] == 0xf0 && x[1] == 0xff && x[2] == 0xff && x[3] == 0xff, "xor_block in-place");

    // 3. secure_zero
    uint8_t buf[32];
    for (size_t i = 0; i < sizeof(buf); ++i) buf[i] = static_cast<uint8_t>(0xa5 + i);
    util::secure_zero(buf, sizeof(buf));
    bool all_zero = true;
    for (uint8_t byte : buf) if (byte != 0) all_zero = false;
    CHECK(all_zero, "secure_zero fills zeros");
    CHECK_NO_THROW(util::secure_zero(nullptr, 0), "secure_zero null safe");
}

int main() {
    std::cout << "--- Unit Test util ---\n";
    test_hex();
    test_secure_random();
    test_file_io();
    test_bytes_operations();
    return test::report();
}
