# Task D: Integrity & CLI Reference

This document provides technical documentation for the CLI tool (`cipher_cli`) and the underlying C++ cryptographic integrity libraries implemented in Task D.

---

## 1. CLI Reference (`cipher_cli`)

### 1.1 Command Syntax

```bash
cipher_cli encrypt [-f|--force] -m <mode> (-k <32 hex> | --key-file <path>) -i <in> -o <out>
cipher_cli decrypt [-f|--force] (-k <32 hex> | --key-file <path>) -i <in> -o <out>
cipher_cli keygen  [-f|--force] -o <path>
cipher_cli -h | --help
```

### 1.2 Commands

| Command | Description |
|:---|:---|
| `encrypt` | Encrypts a file using the Encrypt-then-MAC authenticated container v1 format. |
| `decrypt` | Verifies container integrity and decrypts payload. Mode is auto-detected from the container header. |
| `keygen` | Generates a cryptographically secure 128-bit key (32 hex characters) with POSIX `0600` permissions. |

### 1.3 Options & Flags

| Option | Argument | Description |
|:---|:---|:---|
| `-m` | `<mode>` | Cipher mode: `ecb`, `cbc`, `cfb`, `ofb`, `ctr` (required for `encrypt`; rejected for `decrypt`). |
| `-k` | `<32 hex>` | 128-bit master key supplied directly as 32 hex digits. |
| `--key-file` | `<path>` | Path to key file containing 32 hex digits (whitespace is stripped). **Recommended**. |
| `-i` | `<path>` | Input file path. |
| `-o` | `<path>` | Output file path. |
| `-f`, `--force` | _None_ | Overwrite output file if it already exists (default: refuse to overwrite). |
| `-h`, `--help` | _None_ | Display usage information and exit. |

### 1.4 Exit Codes

| Code | Type | Meaning |
|:---:|:---|:---|
| `0` | Success | Operation succeeded without error. |
| `1` | Usage Error | Missing required options, unknown flag, output file exists without `-f`, or identical `-i` and `-o`. |
| `2` | I/O Error | Missing input or key file, unreadable file, or disk write failure. |
| `3` | Format Error (`FormatError`) | Input is not a valid container (bad magic, unknown version, unsupported mode, size < 38 bytes). |
| `4` | Integrity Failure (`AuthenticationError`) | Integrity check failed: ciphertext or header was tampered with, or wrong decryption key was provided. |

### 1.5 Security Recommendations

1. **Prefer `--key-file` over `-k`**: Passing keys via `-k <hex>` exposes key material in process listing tables (`ps aux`, `/proc`) and shell history files (`~/.bash_history`, `~/.zsh_history`).
2. **File Permissions**: Always ensure key files are stored with permissions `0600` (`chmod 600 <key_file>`). `cipher_cli keygen` enforces this automatically.
3. **Fail-Safe Output Cleanup**: If integrity verification fails (exit code 3 or 4), `cipher_cli` never writes, modifies, or leaves partial files on disk.

---

## 2. C++ Library API Reference

### 2.1 Container & Authenticated Envelope (`include/container.hpp`)

#### `enum class Mode : uint8_t`
Identifies the block cipher mode of operation:
- `Mode::ECB = 1`
- `Mode::CBC = 2`
- `Mode::CFB = 3`
- `Mode::OFB = 4`
- `Mode::CTR = 5`

#### Helper Functions
```cpp
Mode parse_mode(const std::string& str);   // Case-insensitive parser ("ecb".."ctr")
std::string mode_name(Mode mode);          // Returns lowercase string ("ecb".."ctr")
```

#### Exception Classes
- `class AuthenticationError : public std::runtime_error;`
  Thrown when CMAC verification fails (bit-flip, truncated tag, or wrong key).
- `class FormatError : public std::runtime_error;`
  Thrown when the container layout, magic, version, or PKCS#7 padding is invalid.

#### `class SecureEnvelope`
Implements the Encrypt-then-MAC authenticated pipeline.
```cpp
// Constructor: derives encryption and MAC subkeys from master_key
explicit SecureEnvelope(const uint8_t master_key[KEY_SIZE]);

// Seals plaintext into Container v1 binary blob
util::Bytes seal(const util::Bytes& plaintext, Mode mode) const;

// Verifies integrity and opens Container v1 binary blob
util::Bytes open(const util::Bytes& blob) const;
```

---

### 2.2 CMAC & Key Derivation (`include/mac.hpp`)

#### Constants
- `constexpr size_t MAC_TAG_SIZE = 16;` (128-bit authentication tag)

#### `class Cmac`
RFC 4493 / OMAC1 MAC generation and verification over `BlockCipher`.
```cpp
explicit Cmac(const BlockCipher& cipher);

// Streaming / incremental processing
void update(const uint8_t* data, size_t len);
void finalize(uint8_t tag[MAC_TAG_SIZE]);

// One-shot evaluation
static void compute(const BlockCipher& cipher, const uint8_t* data, size_t len, uint8_t tag[MAC_TAG_SIZE]);

// Constant-time verification
static bool verify(const BlockCipher& cipher, const uint8_t* data, size_t len, const uint8_t tag[MAC_TAG_SIZE]);
```

#### `struct DerivedKeys`
Key container with RAII zeroization upon destruction.
```cpp
struct DerivedKeys {
    uint8_t enc[KEY_SIZE]; // Encryption subkey
    uint8_t mac[KEY_SIZE]; // Integrity MAC subkey
};
```

#### `derive_keys`
```cpp
DerivedKeys derive_keys(const BlockCipher& master_cipher);
```
Derives $K_{\text{enc}} = E_{K_{\text{master}}}(0^{15} \,\|\, \text{0x01})$ and $K_{\text{mac}} = E_{K_{\text{master}}}(0^{15} \,\|\, \text{0x02})$.

---

### 2.3 Utilities (`include/util.hpp`)

| Signature | Description |
|:---|:---|
| `void secure_random(uint8_t* out, size_t len);` | Reads cryptographically secure random bytes from OS kernel (`getentropy` / `/dev/urandom`). |
| `Bytes secure_random(size_t len);` | Overload returning random `Bytes` vector. |
| `std::string to_hex(const uint8_t* data, size_t len);` | Formats byte array as lowercase hex string. |
| `std::string to_hex(const Bytes& data);` | Overload formatting `Bytes` vector as hex string. |
| `Bytes from_hex(const std::string& hex);` | Decodes hex string into raw `Bytes`. Throws `std::invalid_argument` on invalid character or odd length. |
| `Bytes read_file(const std::string& path);` | Reads entire file into memory as binary `Bytes`. |
| `void write_file(const std::string& path, const Bytes& data);` | Atomically writes data via temporary file `<path>.tmp` before renaming. |
| `void xor_block(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t len);` | Computes in-place or out-of-place bitwise XOR: `out[i] = a[i] ^ b[i]`. |
| `bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len);` | Compares buffers in constant time without early exit to prevent timing attacks. |
| `void secure_zero(void* p, size_t len);` | Clears memory buffer using `volatile` writes to prevent compiler dead-store elimination. |
