#include "container.hpp"
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

// [PENDING_TASK_C_INTEGRATION]: Temporary PKCS#7 padding placeholder.
// Owned by Task C (padding.hpp). Replace with official function once Task C lands.
util::Bytes pad_pkcs7(const util::Bytes& in) {
    size_t pad = BLOCK_SIZE - (in.size() % BLOCK_SIZE);
    util::Bytes out = in;
    out.insert(out.end(), pad, static_cast<uint8_t>(pad));
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary PKCS#7 unpadding placeholder.
// Owned by Task C (padding.hpp). Replace with official function once Task C lands.
util::Bytes unpad_pkcs7(const util::Bytes& in) {
    if (in.empty() || (in.size() % BLOCK_SIZE) != 0) throw FormatError("Format padding PKCS#7 tidak valid");
    uint8_t pad = in.back();
    if (pad == 0 || pad > BLOCK_SIZE || pad > in.size()) throw FormatError("Format padding PKCS#7 tidak valid");
    for (size_t i = in.size() - pad; i < in.size(); ++i) {
        if (in[i] != pad) throw FormatError("Format padding PKCS#7 tidak valid");
    }
    return util::Bytes(in.begin(), in.end() - pad);
}

// [PENDING_TASK_C_INTEGRATION]: Temporary ECB encrypt placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_ecb_enc(const BlockCipher& c, const uint8_t*, const util::Bytes& in) {
    util::Bytes out(in.size());
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) c.encrypt_block(in.data() + i, out.data() + i);
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary ECB decrypt placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_ecb_dec(const BlockCipher& c, const uint8_t*, const util::Bytes& in) {
    util::Bytes out(in.size());
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) c.decrypt_block(in.data() + i, out.data() + i);
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary CBC mode placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_cbc(const BlockCipher& c, const uint8_t* iv, const util::Bytes& in, bool dec) {
    util::Bytes out(in.size());
    uint8_t block[BLOCK_SIZE];
    const uint8_t* prev = iv;
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        if (dec) {
            c.decrypt_block(in.data() + i, block);
            util::xor_block(block, prev, out.data() + i, BLOCK_SIZE);
            prev = in.data() + i;
        } else {
            util::xor_block(in.data() + i, prev, block, BLOCK_SIZE);
            c.encrypt_block(block, out.data() + i);
            prev = out.data() + i;
        }
    }
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary CFB mode placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_cfb(const BlockCipher& c, const uint8_t* iv, const util::Bytes& in, bool dec) {
    util::Bytes out(in.size());
    uint8_t fb[BLOCK_SIZE], ks[BLOCK_SIZE];
    std::memcpy(fb, iv, BLOCK_SIZE);
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        c.encrypt_block(fb, ks);
        util::xor_block(in.data() + i, ks, out.data() + i, n);
        std::memcpy(fb, (dec ? in.data() : out.data()) + i, n);
    }
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary OFB mode placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_ofb(const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) {
    util::Bytes out(in.size());
    uint8_t st[BLOCK_SIZE];
    std::memcpy(st, iv, BLOCK_SIZE);
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        c.encrypt_block(st, st);
        util::xor_block(in.data() + i, st, out.data() + i, n);
    }
    return out;
}

// [PENDING_TASK_C_INTEGRATION]: Temporary CTR mode placeholder.
// Owned by Task C (modes.hpp). Replace with official function once Task C lands.
util::Bytes mock_ctr(const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) {
    util::Bytes out(in.size());
    uint8_t ctr[BLOCK_SIZE], ks[BLOCK_SIZE];
    std::memcpy(ctr, iv, BLOCK_SIZE);
    for (size_t i = 0; i < in.size(); i += BLOCK_SIZE) {
        size_t n = std::min<size_t>(BLOCK_SIZE, in.size() - i);
        c.encrypt_block(ctr, ks);
        util::xor_block(in.data() + i, ks, out.data() + i, n);
        for (int j = BLOCK_SIZE - 1; j >= 0 && ++ctr[j] == 0; --j) {}
    }
    return out;
}

using ModeFn = util::Bytes (*)(const BlockCipher&, const uint8_t*, const util::Bytes&);
struct ModeHandler {
    bool requires_padding;
    ModeFn encrypt;
    ModeFn decrypt;
};

// [PENDING_TASK_C_INTEGRATION]: Mode operations currently use an internal mock adapter.
// Once Task C (modes.hpp & padding.hpp) lands on main, plug official functions into container.cpp.
const ModeHandler& get_handler(Mode mode) {
    static const ModeHandler handlers[] = {
        { true,  mock_ecb_enc, mock_ecb_dec },
        { true,  [](const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) { return mock_cbc(c, iv, in, false); },
                 [](const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) { return mock_cbc(c, iv, in, true); } },
        { false, [](const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) { return mock_cfb(c, iv, in, false); },
                 [](const BlockCipher& c, const uint8_t* iv, const util::Bytes& in) { return mock_cfb(c, iv, in, true); } },
        { false, mock_ofb, mock_ofb },
        { false, mock_ctr, mock_ctr }
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

util::Bytes SecureEnvelope::seal(const util::Bytes& plaintext, Mode mode) const {
    const auto& handler = get_handler(mode);
    uint8_t iv[BLOCK_SIZE] = {0};
    if (mode != Mode::ECB) util::secure_random(iv, BLOCK_SIZE);

    util::Bytes payload = handler.requires_padding ? pad_pkcs7(plaintext) : plaintext;
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
    return handler.requires_padding ? unpad_pkcs7(plaintext) : plaintext;
}
