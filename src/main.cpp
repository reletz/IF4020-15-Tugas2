#include "cipher.hpp"
#include "container.hpp"
#include "util.hpp"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

struct CliError : std::runtime_error { using std::runtime_error::runtime_error; };
struct IoError  : std::runtime_error { using std::runtime_error::runtime_error; };

void print_help(std::ostream& os = std::cout) {
    os << "Usage:\n"
       << "  cipher_cli encrypt [-f|--force] -m <mode> (-k <32 hex> | --key-file <path>) -i <in> -o <out>\n"
       << "  cipher_cli decrypt [-f|--force] (-k <32 hex> | --key-file <path>) -i <in> -o <out>\n"
       << "  cipher_cli keygen  [-f|--force] -o <path>\n"
       << "  cipher_cli -h | --help\n\n"
       << "Commands:\n"
       << "  encrypt    Encrypt a file into an authenticated container (v1).\n"
       << "  decrypt    Verify integrity and decrypt an authenticated container.\n"
       << "  keygen     Generate a random 128-bit key (32 hex characters) with mode 0600.\n\n"
       << "Options:\n"
       << "  -m <mode>          Cipher mode: ecb, cbc, cfb, ofb, ctr (encrypt only)\n"
       << "  -k <32 hex>        Provide 128-bit key directly as 32 hex characters\n"
       << "  --key-file <path>  Read 128-bit key from file (recommended over -k to avoid leaking keys)\n"
       << "  -i <path>          Input file path\n"
       << "  -o <path>          Output file path\n"
       << "  -f, --force        Overwrite output file if it already exists\n"
       << "  -h, --help         Show this help message\n\n"
       << "Examples:\n"
       << "  1. Generate key file:\n"
       << "     cipher_cli keygen -o secret.key\n\n"
       << "  2. Encrypt using key file:\n"
       << "     cipher_cli encrypt -m cbc --key-file secret.key -i file.pdf -o file.enc\n\n"
       << "  3. Decrypt using key file (mode is auto-detected from header):\n"
       << "     cipher_cli decrypt --key-file secret.key -i file.enc -o file.pdf\n\n"
       << "  4. Encrypt using raw 32-hex key directly:\n"
       << "     cipher_cli encrypt -m ctr -k 00112233445566778899aabbccddeeff -i file.txt -o file.enc\n";
}

void load_key(std::string& k_hex, const std::string& k_file, uint8_t key[KEY_SIZE]) {
    if (k_hex.empty() == k_file.empty()) {
        throw CliError("Specify exactly one of -k <32 hex> or --key-file <path>");
    }
    std::string hex = k_hex;
    util::secure_zero(k_hex.data(), k_hex.size());

    if (!k_file.empty()) {
        try {
            auto b = util::read_file(k_file);
            hex.assign(b.begin(), b.end());
            util::secure_zero(b.data(), b.size());
        } catch (const std::exception& e) {
            throw IoError(e.what());
        }
        while (!hex.empty() && std::isspace(static_cast<unsigned char>(hex.back()))) hex.pop_back();
        while (!hex.empty() && std::isspace(static_cast<unsigned char>(hex.front()))) hex.erase(0, 1);
    }

    if (hex.size() != 32) {
        util::secure_zero(hex.data(), hex.size());
        throw CliError("Invalid key: must be exactly 32 hexadecimal characters (128 bits)");
    }

    try {
        auto raw = util::from_hex(hex);
        std::memcpy(key, raw.data(), KEY_SIZE);
        util::secure_zero(raw.data(), raw.size());
    } catch (const std::exception&) {
        util::secure_zero(hex.data(), hex.size());
        throw CliError("Invalid key: contains non-hexadecimal characters");
    }
    util::secure_zero(hex.data(), hex.size());
}

void validate_paths(const std::string& in, const std::string& out, bool force) {
    if (in.empty()) throw CliError("Missing required option: -i <in>");
    if (out.empty()) throw CliError("Missing required option: -o <out>");
    std::error_code ec;
    if (fs::exists(in, ec) && fs::exists(out, ec) && fs::equivalent(in, out, ec)) {
        throw CliError("Input and output resolve to the same canonical path: " + in);
    }
    if (!force && fs::exists(out, ec)) {
        throw CliError("Output file '" + out + "' already exists. Use -f or --force to overwrite.");
    }
}

void do_keygen(const std::string& out, bool force) {
    if (out.empty()) throw CliError("Missing required option: -o <path>");
    std::error_code ec;
    if (!force && fs::exists(out, ec)) {
        throw CliError("Output file '" + out + "' already exists. Use -f or --force to overwrite.");
    }

    uint8_t key[KEY_SIZE];
    util::secure_random(key, KEY_SIZE);
    std::string hex_str = util::to_hex(key, KEY_SIZE) + "\n";
    util::secure_zero(key, sizeof(key));

    int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        util::secure_zero(hex_str.data(), hex_str.size());
        throw IoError("Failed to create key file '" + out + "': " + std::strerror(errno));
    }
    ::fchmod(fd, S_IRUSR | S_IWUSR);
    ssize_t written = ::write(fd, hex_str.data(), hex_str.size());
    int write_err = errno;
    ::close(fd);
    util::secure_zero(hex_str.data(), hex_str.size());

    if (written < 0 || static_cast<size_t>(written) != 33) {
        fs::remove(out, ec);
        throw IoError("Failed to write key file '" + out + "': " + std::strerror(write_err));
    }
    std::cout << "Key generated successfully.\nOutput file: " << out << " (permissions 0600)\n";
}

void do_cipher(bool encrypt, const std::string& mode_str, std::string& k_hex,
               const std::string& k_file, const std::string& in_path, const std::string& out_path, bool force) {
    validate_paths(in_path, out_path, force);

    uint8_t key[KEY_SIZE] = {0};
    load_key(k_hex, k_file, key);

    util::Bytes in_data;
    try {
        in_data = util::read_file(in_path);
    } catch (const std::exception& e) {
        util::secure_zero(key, sizeof(key));
        throw IoError(e.what());
    }

    SecureEnvelope env(key);
    util::secure_zero(key, sizeof(key));

    util::Bytes out_data;
    Mode mode;
    if (encrypt) {
        if (mode_str.empty()) throw CliError("Missing required option: -m <mode>");
        try {
            mode = parse_mode(mode_str);
        } catch (const std::exception&) {
            throw CliError("Unsupported mode: '" + mode_str + "'. Supported modes: ecb, cbc, cfb, ofb, ctr");
        }
        out_data = env.seal(in_data, mode);
    } else {
        if (!mode_str.empty()) throw CliError("Option -m is not allowed for decrypt (mode is auto-detected)");
        out_data = env.open(in_data);
        mode = static_cast<Mode>(in_data[5]);
    }

    try {
        util::write_file(out_path, out_data);
    } catch (const std::exception& e) {
        std::error_code ec;
        fs::remove(out_path, ec);
        throw IoError(e.what());
    }

    std::cout << (encrypt ? "Encryption" : "Decryption") << " successful.\n"
              << "Mode:        " << mode_name(mode) << "\n"
              << "Input size:  " << in_data.size() << " bytes\n"
              << "Output size: " << out_data.size() << " bytes\n";

    util::secure_zero(in_data.data(), in_data.size());
    util::secure_zero(out_data.data(), out_data.size());
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No command specified.\n\n";
        print_help(std::cerr);
        return 1;
    }

    std::string_view cmd = argv[1];
    if (cmd == "-h" || cmd == "--help") {
        print_help(std::cout);
        return 0;
    }

    std::string mode, key_hex, key_file, in_file, out_file;
    bool force = false;

    try {
        for (int i = 2; i < argc; ++i) {
            std::string_view arg = argv[i];
            auto need_val = [&](std::string_view opt) {
                if (++i >= argc) throw CliError("Option " + std::string(opt) + " requires an argument");
                return argv[i];
            };

            if (arg == "-h" || arg == "--help") {
                print_help(std::cout);
                return 0;
            } else if (arg == "-f" || arg == "--force") {
                force = true;
            } else if (arg == "-m") {
                mode = need_val("-m");
            } else if (arg == "-k") {
                key_hex = need_val("-k");
                util::secure_zero(argv[i], std::strlen(argv[i]));
            } else if (arg == "--key-file") {
                key_file = need_val("--key-file");
            } else if (arg == "-i") {
                in_file = need_val("-i");
            } else if (arg == "-o") {
                out_file = need_val("-o");
            } else {
                throw CliError("Unknown option: " + std::string(arg));
            }
        }

        if (cmd == "encrypt") {
            do_cipher(true, mode, key_hex, key_file, in_file, out_file, force);
        } else if (cmd == "decrypt") {
            do_cipher(false, mode, key_hex, key_file, in_file, out_file, force);
        } else if (cmd == "keygen") {
            if (!mode.empty() || !key_hex.empty() || !key_file.empty() || !in_file.empty()) {
                throw CliError("keygen accepts only [-f|--force] and -o <path>");
            }
            do_keygen(out_file, force);
        } else {
            throw CliError("Unknown command '" + std::string(cmd) + "'");
        }
        return 0;

    } catch (const FormatError&) {
        std::cerr << "file is not a valid/supported ciphertext" << std::endl;
        return 3;
    } catch (const AuthenticationError&) {
        std::cerr << "integrity check failed: wrong key or file was modified" << std::endl;
        return 4;
    } catch (const IoError& e) {
        std::cerr << e.what() << std::endl;
        return 2;
    } catch (const CliError& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        print_help(std::cerr);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
