#include "padding.hpp"
#include "test_helpers.hpp"

#include <string>
#include <vector>

void test_pad_lengths_and_contents() {
    const size_t test_lens[] = {0, 1, 15, 16, 17, 31, 32};
    for (size_t len : test_lens) {
        util::Bytes in(len);
        for (size_t i = 0; i < len; ++i) {
            in[i] = static_cast<uint8_t>((i + 1) & 0xFF);
        }

        util::Bytes padded = pkcs7_pad(in);
        size_t expected_len = ((len / BLOCK_SIZE) + 1) * BLOCK_SIZE;
        size_t expected_pad_val = expected_len - len;

        CHECK(padded.size() == expected_len, "Panjang padded sesuai kelipatan 16 untuk len " + std::to_string(len));

        // Verifikasi prefiks identik dengan input
        bool prefix_ok = true;
        for (size_t i = 0; i < len; ++i) {
            if (padded[i] != in[i]) {
                prefix_ok = false;
                break;
            }
        }
        CHECK(prefix_ok, "Prefiks padded identik dengan data asli untuk len " + std::to_string(len));

        // Verifikasi semua byte padding bernilai sama dengan expected_pad_val
        bool pad_bytes_ok = true;
        for (size_t i = len; i < expected_len; ++i) {
            if (padded[i] != static_cast<uint8_t>(expected_pad_val)) {
                pad_bytes_ok = false;
                break;
            }
        }
        CHECK(pad_bytes_ok, "Nilai byte padding tepat sama dengan panjang padding untuk len " + std::to_string(len));
    }
}

void test_roundtrip_all_lengths() {
    for (size_t len = 0; len <= 100; ++len) {
        util::Bytes in(len);
        for (size_t i = 0; i < len; ++i) {
            in[i] = static_cast<uint8_t>((i * 37 + 11) & 0xFF);
        }

        util::Bytes padded = pkcs7_pad(in);
        util::Bytes unpadded = pkcs7_unpad(padded);

        CHECK(unpadded == in, "Round-trip unpad(pad(x)) == x untuk len " + std::to_string(len));
    }
}

void test_unpad_invalid_inputs() {
    // 1. Input kosong
    CHECK_THROWS(pkcs7_unpad(util::Bytes{}), PaddingError, "unpad menolak input kosong");

    // 2. Panjang bukan kelipatan 16
    CHECK_THROWS(pkcs7_unpad(util::Bytes{0x01}), PaddingError, "unpad menolak panjang 1");
    CHECK_THROWS(pkcs7_unpad(util::Bytes(15, 0x01)), PaddingError, "unpad menolak panjang 15");
    CHECK_THROWS(pkcs7_unpad(util::Bytes(17, 0x01)), PaddingError, "unpad menolak panjang 17");
    CHECK_THROWS(pkcs7_unpad(util::Bytes(31, 0x01)), PaddingError, "unpad menolak panjang 31");

    // 3. Byte terakhir 0x00
    util::Bytes pad_zero(16, 0x00);
    CHECK_THROWS(pkcs7_unpad(pad_zero), PaddingError, "unpad menolak byte terakhir 0x00");

    // 4. Byte terakhir > 0x10
    util::Bytes pad_too_large(16, 0x11);
    CHECK_THROWS(pkcs7_unpad(pad_too_large), PaddingError, "unpad menolak byte terakhir 0x11");
    util::Bytes pad_ff(16, 0xFF);
    CHECK_THROWS(pkcs7_unpad(pad_ff), PaddingError, "unpad menolak byte terakhir 0xFF");

    // 5. Byte padding tidak seragam (misal: ... 03 03 02)
    util::Bytes non_uniform(16, 0xAA);
    non_uniform[13] = 0x03;
    non_uniform[14] = 0x03;
    non_uniform[15] = 0x02;
    CHECK_THROWS(pkcs7_unpad(non_uniform), PaddingError, "unpad menolak padding tidak seragam (ujung 02 preceded by 03)");

    util::Bytes corrupted_three(16, 0xAA);
    corrupted_three[13] = 0x01;
    corrupted_three[14] = 0x03;
    corrupted_three[15] = 0x03;
    CHECK_THROWS(pkcs7_unpad(corrupted_three), PaddingError, "unpad menolak pad 3 byte dengan byte awal rusak");

    // 6. Seluruh blok 16 byte tapi byte pertamanya bukan 0x10
    util::Bytes almost_full(16, 0x10);
    almost_full[0] = 0x0F;
    CHECK_THROWS(pkcs7_unpad(almost_full), PaddingError, "unpad menolak blok pad 16 yang tidak seragam");
}

int main() {
    test_pad_lengths_and_contents();
    test_roundtrip_all_lengths();
    test_unpad_invalid_inputs();
    return test::report();
}
