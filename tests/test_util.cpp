#include "util.hpp"
#include "test_helpers.hpp"

#include <cstdio>
#include <numeric>
#include <sys/stat.h>
#include <vector>

void test_hex() {
    // 1. Round-trip empty
    {
        util::Bytes empty;
        std::string hex = util::to_hex(empty);
        CHECK(hex.empty(), "to_hex dari Bytes kosong harus menghasilkan string kosong");
        util::Bytes back = util::from_hex(hex);
        CHECK(back.empty(), "from_hex string kosong harus menghasilkan Bytes kosong");
    }

    // 2. Round-trip bytes arbitrer
    {
        util::Bytes data = {0x00, 0x01, 0x0a, 0x0f, 0x10, 0x7f, 0x80, 0xfe, 0xff};
        std::string hex = util::to_hex(data);
        CHECK(hex == "00010a0f107f80feff", "to_hex harus menghasilkan string lowercase yang benar");
        util::Bytes back = util::from_hex(hex);
        CHECK(back == data, "from_hex round-trip harus identik dengan data asli");
    }

    // 3. Round-trip seluruh 256 nilai byte
    {
        util::Bytes all_bytes(256);
        for (size_t i = 0; i < 256; ++i) {
            all_bytes[i] = static_cast<uint8_t>(i);
        }
        std::string hex = util::to_hex(all_bytes);
        CHECK(hex.length() == 512, "Panjang hex untuk 256 byte harus 512 karakter");
        util::Bytes back = util::from_hex(hex);
        CHECK(back == all_bytes, "Round-trip 256 byte harus identik");
    }

    // 4. from_hex menolak panjang ganjil
    CHECK_THROWS(util::from_hex("abc"), std::invalid_argument,
                 "from_hex(\"abc\") harus melempar std::invalid_argument (panjang ganjil)");
    CHECK_THROWS(util::from_hex("a"), std::invalid_argument,
                 "from_hex(\"a\") harus melempar std::invalid_argument (panjang ganjil)");

    // 5. from_hex menolak karakter non-hex
    CHECK_THROWS(util::from_hex("zz"), std::invalid_argument,
                 "from_hex(\"zz\") harus melempar std::invalid_argument (karakter non-hex)");
    CHECK_THROWS(util::from_hex("0g"), std::invalid_argument,
                 "from_hex(\"0g\") harus melempar std::invalid_argument");
    CHECK_THROWS(util::from_hex("1 "), std::invalid_argument,
                 "from_hex dengan spasi harus melempar std::invalid_argument");

    // Pesan error tidak boleh membocorkan konten input (FIX 1)
    {
        bool caught = false;
        try {
            util::from_hex("00zz");
        } catch (const std::invalid_argument& e) {
            caught = true;
            std::string msg = e.what();
            CHECK(msg.find("zz") == std::string::npos,
                  "Pesan exception from_hex tidak boleh memuat konten input ('zz')");
        }
        CHECK(caught, "from_hex(\"00zz\") harus melempar std::invalid_argument");
    }

    // 6. from_hex menerima huruf kapital dan mixed case
    {
        util::Bytes expected = {0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f};
        util::Bytes upper = util::from_hex("0A1B2C3D4E5F");
        CHECK(upper == expected, "from_hex harus menerima huruf kapital");

        util::Bytes mixed = util::from_hex("0a1B2c3D4e5F");
        CHECK(mixed == expected, "from_hex harus menerima mixed case");
    }
}

void test_secure_random() {
    // 1. Ukuran dan perbedaan dua pemanggilan berurutan
    util::Bytes r1 = util::secure_random(16);
    util::Bytes r2 = util::secure_random(16);

    CHECK(r1.size() == 16, "secure_random(16) harus menghasilkan 16 byte");
    CHECK(r2.size() == 16, "secure_random(16) harus menghasilkan 16 byte");
    CHECK(r1 != r2, "Dua pemanggilan secure_random(16) berturut-turut harus menghasilkan byte berbeda");

    // 2. secure_random dengan ukuran 0
    util::Bytes r0 = util::secure_random(0);
    CHECK(r0.empty(), "secure_random(0) harus menghasilkan vektor kosong");

    // 3. secure_random melewati batas 256-byte chunk loop
    util::Bytes r_large = util::secure_random(600);
    CHECK(r_large.size() == 600, "secure_random(600) harus menghasilkan 600 byte");

    // 4. Pointer null saat len > 0 melempar invalid_argument
    CHECK_THROWS(util::secure_random(nullptr, 16), std::invalid_argument,
                 "secure_random(nullptr, 16) harus melempar std::invalid_argument");
}

void test_file_io() {
    test::TempDir temp_dir;
    const std::string path_0 = temp_dir.path() + "/test_tmp_0byte.bin";
    const std::string path_1 = temp_dir.path() + "/test_tmp_1byte.bin";
    const std::string path_256 = temp_dir.path() + "/test_tmp_256byte.bin";
    const std::string path_1mb = temp_dir.path() + "/test_tmp_1mb.bin";

    // 1. Berkas 0 byte
    {
        util::Bytes empty;
        util::write_file(path_0, empty);
        util::Bytes back = util::read_file(path_0);
        CHECK(back.empty(), "Round-trip berkas 0 byte harus menghasilkan Bytes kosong");
        std::remove(path_0.c_str());
    }

    // 2. Berkas 1 byte
    {
        util::Bytes one_byte = {0x7e};
        util::write_file(path_1, one_byte);
        util::Bytes back = util::read_file(path_1);
        CHECK(back == one_byte, "Round-trip berkas 1 byte harus cocok");
        std::remove(path_1.c_str());
    }

    // 3. Seluruh 256 nilai byte
    {
        util::Bytes all_bytes(256);
        for (size_t i = 0; i < 256; ++i) {
            all_bytes[i] = static_cast<uint8_t>(i);
        }
        util::write_file(path_256, all_bytes);
        util::Bytes back = util::read_file(path_256);
        CHECK(back == all_bytes, "Round-trip berkas 256 byte harus cocok");
        std::remove(path_256.c_str());
    }

    // 4. Berkas ~1 MiB acak
    {
        const size_t one_mib = 1024 * 1024;
        util::Bytes large_data = util::secure_random(one_mib);
        util::write_file(path_1mb, large_data);
        util::Bytes back = util::read_file(path_1mb);
        CHECK(back.size() == one_mib, "Ukuran berkas 1 MiB yang dibaca kembali harus tepat");
        CHECK(back == large_data, "Konten berkas 1 MiB harus identik dengan data asli");
        std::remove(path_1mb.c_str());
    }

    // 5. read_file pada berkas yang tidak ada harus melempar exception dengan path dalam pesan
    {
        const std::string non_existent = temp_dir.path() + "/file_yang_pasti_tidak_ada_xyz123.bin";
        bool caught = false;
        try {
            util::read_file(non_existent);
        } catch (const std::runtime_error& e) {
            caught = true;
            std::string msg = e.what();
            CHECK(msg.find(non_existent) != std::string::npos,
                  "Pesan exception read_file harus memuat path berkas yang dicari");
        }
        CHECK(caught, "read_file pada file yang tidak ada harus melempar std::runtime_error");
    }

    // 6. write_file ke path di dalam direktori yang tidak ada harus melempar std::runtime_error
    //    dan tidak meninggalkan target maupun <target>.tmp (FIX 4)
    {
        const std::string bad_path = temp_dir.path() + "/non_existent_subdir/target.bin";
        const std::string bad_tmp = bad_path + ".tmp";
        bool caught = false;
        try {
            util::write_file(bad_path, util::Bytes{0x01, 0x02});
        } catch (const std::runtime_error&) {
            caught = true;
        }
        CHECK(caught, "write_file ke direktori yang tidak ada harus melempar std::runtime_error");

        struct stat st;
        bool target_exists = (::stat(bad_path.c_str(), &st) == 0);
        CHECK(!target_exists, "Berkas target tidak boleh ada setelah write_file gagal");

        bool tmp_exists = (::stat(bad_tmp.c_str(), &st) == 0);
        CHECK(!tmp_exists, "Berkas .tmp tidak boleh ada setelah write_file gagal");
    }
}

void test_bytes_operations() {
    // 1. constant_time_equal
    {
        uint8_t a[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        uint8_t b[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        CHECK(util::constant_time_equal(a, b, 16), "Buffer identik harus bernilai true");

        // Perbedaan di byte terakhir
        b[15] = 99;
        CHECK(!util::constant_time_equal(a, b, 16), "Perbedaan di byte terakhir harus bernilai false");

        // Perbedaan di byte pertama
        b[15] = 16;
        b[0] = 99;
        CHECK(!util::constant_time_equal(a, b, 16), "Perbedaan di byte pertama harus bernilai false");

        // Perbedaan di byte tengah
        b[0] = 1;
        b[7] = 99;
        CHECK(!util::constant_time_equal(a, b, 16), "Perbedaan di byte tengah harus bernilai false");

        // Panjang 0
        CHECK(util::constant_time_equal(nullptr, nullptr, 0), "Panjang 0 harus bernilai true");
        CHECK(util::constant_time_equal(a, b, 0), "Panjang 0 harus bernilai true");
    }

    // 2. xor_block
    {
        uint8_t a[4] = {0x0f, 0xf0, 0xaa, 0x55};
        uint8_t b[4] = {0xff, 0x0f, 0x55, 0xaa};
        uint8_t out[4] = {0};

        util::xor_block(a, b, out, 4);
        CHECK(out[0] == 0xf0 && out[1] == 0xff && out[2] == 0xff && out[3] == 0xff,
              "xor_block menghasilkan nilai yang benar");

        // In-place XOR (out == a)
        util::xor_block(a, b, a, 4);
        CHECK(a[0] == 0xf0 && a[1] == 0xff && a[2] == 0xff && a[3] == 0xff,
              "xor_block in-place (out == a) harus bekerja dengan benar");

        // Self XOR (a ^ a == 0)
        util::xor_block(a, a, out, 4);
        CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0,
              "xor_block self-XOR (a ^ a) harus bernilai 0");
    }

    // 3. secure_zero
    {
        uint8_t buf[32];
        for (size_t i = 0; i < sizeof(buf); ++i) {
            buf[i] = static_cast<uint8_t>(0xa5 + i);
        }

        util::secure_zero(buf, sizeof(buf));

        bool all_zero = true;
        for (size_t i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 0) {
                all_zero = false;
                break;
            }
        }
        CHECK(all_zero, "secure_zero harus mengisi seluruh buffer dengan nilai 0");

        // Memastikan pointer null atau len 0 aman
        CHECK_NO_THROW(util::secure_zero(nullptr, 0), "secure_zero(nullptr, 0) tidak boleh melempar");
        CHECK_NO_THROW(util::secure_zero(buf, 0), "secure_zero(buf, 0) tidak boleh melempar");
    }
}

int main() {
    std::cout << "--- Menjalankan Pengujian Unit util ---\n";
    test_hex();
    test_secure_random();
    test_file_io();
    test_bytes_operations();

    return test::report();
}
