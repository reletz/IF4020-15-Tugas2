#include "util.hpp"

#include <sys/stat.h>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace util {

Bytes read_file(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        throw std::runtime_error("read_file: Berkas tidak ditemukan atau tidak dapat diakses: " + path);
    }
    if (S_ISDIR(st.st_mode)) {
        throw std::runtime_error("read_file: Jalur mengarah ke direktori, bukan berkas reguler: " + path);
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open() || file.fail()) {
        throw std::runtime_error("read_file: Gagal membuka berkas: " + path);
    }

    auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("read_file: Gagal membaca ukuran berkas: " + path);
    }

    if (size == 0) {
        return Bytes{};
    }

    Bytes buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("read_file: Gagal membaca isi berkas: " + path);
    }

    return buffer;
}

void write_file(const std::string& path, const Bytes& data) {
    std::string tmp_path = path + ".tmp";

    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open() || file.fail()) {
            throw std::runtime_error("write_file: Gagal membuat berkas sementara: " + tmp_path);
        }

        if (!data.empty()) {
            file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (file.fail()) {
                file.close();
                std::remove(tmp_path.c_str());
                throw std::runtime_error("write_file: Gagal menulis data ke berkas: " + tmp_path);
            }
        }

        file.close();
        if (file.fail()) {
            std::remove(tmp_path.c_str());
            throw std::runtime_error("write_file: Gagal menutup berkas sementara: " + tmp_path);
        }
    }

    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        throw std::runtime_error("write_file: Gagal memindahkan berkas sementara ke target: " + path);
    }
}

} // namespace util
