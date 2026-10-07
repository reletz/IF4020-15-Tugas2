#include "container.hpp"
#include "modes.hpp"
#include "padding.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

Mode parse_mode(const std::string& str) {
    std::string s;
    for (char c : str) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s == "ecb") return Mode::ECB;
    if (s == "cbc") return Mode::CBC;
    if (s == "cfb") return Mode::CFB;
    if (s == "ofb") return Mode::OFB;
    if (s == "ctr") return Mode::CTR;
    throw std::invalid_argument("Mode tidak valid: '" + str + "'");
}

std::string mode_name(Mode mode) {
    switch (mode) {
        case Mode::ECB: return "ecb";
        case Mode::CBC: return "cbc";
        case Mode::CFB: return "cfb";
        case Mode::OFB: return "ofb";
        case Mode::CTR: return "ctr";
        default: throw std::invalid_argument("Mode ID tidak valid");
    }
}

namespace {

using ModeFn = util::Bytes (*)(const BlockCipher&, const uint8_t*, const util::Bytes&);
struct ModeHandler {
    bool requires_padding;
    ModeFn encrypt;
    ModeFn decrypt;
};

const ModeHandler& get_handler(Mode mode) {
    static const ModeHandler handlers[] = {
        { true,  modes::ecb_encrypt, modes::ecb_decrypt },
        { true,  modes::cbc_encrypt, modes::cbc_decrypt },
        { false, modes::cfb_encrypt, modes::cfb_decrypt },
        { false, modes::ofb_crypt,   modes::ofb_crypt },
        { false, modes::ctr_crypt,   modes::ctr_crypt }
    };
    auto idx = static_cast<size_t>(mode);
    if (idx < 1 || idx > 5) throw FormatError("Mode ID tidak valid");
    return handlers[idx - 1];
}

} // namespace

SecureEnvelope::SecureEnvelope(const uint8_t master_key[KEY_SIZE])
    : SecureEnvelope(derive_keys(CustomCipher(master_key))) {}

SecureEnvelope::SecureEnvelope(DerivedKeys keys)
    : enc_cipher_(keys.enc), mac_cipher_(keys.mac) {
    util::secure_zero(keys.enc, sizeof(keys.enc));
    util::secure_zero(keys.mac, sizeof(keys.mac));
}

util::Bytes SecureEnvelope::seal(const util::Bytes& plaintext, Mode mode, const uint8_t* custom_iv) const {
    const auto& handler = get_handler(mode);
    uint8_t iv[BLOCK_SIZE] = {0};
    if (mode != Mode::ECB) {
        if (custom_iv) {
            std::memcpy(iv, custom_iv, BLOCK_SIZE);
        } else {
            util::secure_random(iv, BLOCK_SIZE);
        }
    }

    util::Bytes payload = handler.requires_padding ? pkcs7_pad(plaintext) : plaintext;
    util::Bytes ciphertext = handler.encrypt(enc_cipher_, iv, payload);

    util::Bytes blob;
    blob.reserve(CONTAINER_HEADER_SIZE + ciphertext.size() + MAC_TAG_SIZE);
    blob.insert(blob.end(), {'I', 'F', '1', '5', 0x01, static_cast<uint8_t>(mode)});
    blob.insert(blob.end(), iv, iv + BLOCK_SIZE);
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());

    uint8_t tag[MAC_TAG_SIZE];
    Cmac::compute(mac_cipher_, blob.data(), blob.size(), tag);
    blob.insert(blob.end(), tag, tag + MAC_TAG_SIZE);
    return blob;
}

util::Bytes SecureEnvelope::open(const util::Bytes& blob) const {
    if (blob.size() < CONTAINER_MIN_SIZE ||
        blob[0] != 'I' || blob[1] != 'F' || blob[2] != '1' || blob[3] != '5' ||
        blob[4] != 0x01 || blob[5] < 1 || blob[5] > 5) {
        throw FormatError("Format kontainer atau header tidak valid");
    }

    size_t n_len = blob.size() - CONTAINER_MIN_SIZE;
    size_t auth_len = CONTAINER_HEADER_SIZE + n_len;
    if (!Cmac::verify(mac_cipher_, blob.data(), auth_len, blob.data() + auth_len)) {
        throw AuthenticationError("Gagal verifikasi integritas: data korup atau kunci salah");
    }

    Mode mode = static_cast<Mode>(blob[5]);
    const auto& handler = get_handler(mode);
    if (handler.requires_padding && (n_len < BLOCK_SIZE || (n_len % BLOCK_SIZE) != 0)) {
        throw FormatError("Panjang payload tidak valid untuk mode blok terpad");
    }

    const uint8_t* iv = blob.data() + 6;
    util::Bytes ciphertext(blob.begin() + CONTAINER_HEADER_SIZE, blob.begin() + auth_len);
    util::Bytes plaintext = handler.decrypt(enc_cipher_, iv, ciphertext);
    if (handler.requires_padding) {
        try {
            return pkcs7_unpad(plaintext);
        } catch (const PaddingError& e) {
            throw FormatError(std::string("Format padding tidak valid: ") + e.what());
        }
    }
    return plaintext;
}
