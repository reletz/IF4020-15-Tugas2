#!/usr/bin/env bash
set -euo pipefail

# End-to-end integration and system tests for cipher_cli.
CLI="${CLI:-./cipher_cli}"
if [ ! -x "$CLI" ]; then
    echo "[ERROR] Executable $CLI not found. Run 'make app' first." >&2
    exit 1
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

PASS_COUNT=0
pass() {
    PASS_COUNT=$(( PASS_COUNT + 1 ))
    echo "  [PASS] $1"
}

flip_bit() {
    local file="$1" off="$2"
    local byte
    byte=$(od -An -tu1 -j "$off" -N1 "$file" | tr -d ' ')
    printf "$(printf '\\%03o' $(( byte ^ 1 )))" | dd of="$file" bs=1 seek="$off" conv=notrunc 2>/dev/null
}

assert_decrypt_fail() {
    local key="$1" in_file="$2" expected="$3" msg="$4"
    local out_file="$TMP_DIR/fail_out.bin"
    rm -f "$out_file"
    set +e
    "$CLI" decrypt -k "$key" -i "$in_file" -o "$out_file" >/dev/null 2>&1
    local rc=$?
    set -e
    if [[ ! " $expected " =~ " $rc " ]]; then
        echo "[FAIL] $msg: expected exit code ($expected), got $rc" >&2
        exit 1
    fi
    if [ -f "$out_file" ]; then
        echo "[FAIL] $msg: output file exists despite failure!" >&2
        exit 1
    fi
    pass "$msg"
}

# 0. Setup test keys
"$CLI" keygen -o "$TMP_DIR/key.hex" >/dev/null
KEY=$(tr -d '\r\n' < "$TMP_DIR/key.hex")
if [ "${KEY:0:2}" = "00" ]; then WRONG_KEY="ff${KEY:2}"; else WRONG_KEY="00${KEY:2}"; fi
MODES=("ecb" "cbc" "cfb" "ofb" "ctr")

echo "=== 1. Round-Trip Tests (All 5 Modes) ==="

# 1a. Empty file (0 bytes)
touch "$TMP_DIR/empty.bin"
for m in "${MODES[@]}"; do
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$TMP_DIR/empty.bin" -o "$TMP_DIR/empty_${m}.enc" >/dev/null
    "$CLI" decrypt -k "$KEY" -i "$TMP_DIR/empty_${m}.enc" -o "$TMP_DIR/empty_${m}.dec" >/dev/null
    cmp -s "$TMP_DIR/empty.bin" "$TMP_DIR/empty_${m}.dec"
    pass "Empty file (0B) round-trip in $m mode"
done

# 1b. Boundary sizes (1, 15, 16, 17, 31, 32, 33 bytes)
for sz in 1 15 16 17 31 32 33; do
    f="$TMP_DIR/bound_${sz}.bin"
    dd if=/dev/urandom of="$f" bs=1 count="$sz" 2>/dev/null
    for m in "${MODES[@]}"; do
        "$CLI" encrypt -m "$m" -k "$KEY" -i "$f" -o "$TMP_DIR/bound_${sz}_${m}.enc" >/dev/null
        "$CLI" decrypt -k "$KEY" -i "$TMP_DIR/bound_${sz}_${m}.enc" -o "$TMP_DIR/bound_${sz}_${m}.dec" >/dev/null
        cmp -s "$f" "$TMP_DIR/bound_${sz}_${m}.dec"
    done
    pass "Boundary size ${sz}B round-trip across all modes"
done

# 1c. Arbitrary binary (all 256 byte values 0x00..0xFF, nulls, newlines)
ALL256="$TMP_DIR/all256.bin"
for ((i = 0; i < 256; i++)); do printf "\\$(printf '%03o' "$i")"; done > "$ALL256"
for m in "${MODES[@]}"; do
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$ALL256" -o "$TMP_DIR/all256_${m}.enc" >/dev/null
    "$CLI" decrypt -k "$KEY" -i "$TMP_DIR/all256_${m}.enc" -o "$TMP_DIR/all256_${m}.dec" >/dev/null
    cmp -s "$ALL256" "$TMP_DIR/all256_${m}.dec"
    pass "Arbitrary binary (256B pattern) round-trip in $m mode"
done

# 1d. Multi-megabyte (2 MiB random binary)
MB2="$TMP_DIR/2mb.bin"
dd if=/dev/urandom of="$MB2" bs=1048576 count=2 2>/dev/null
for m in "${MODES[@]}"; do
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$MB2" -o "$TMP_DIR/2mb_${m}.enc" >/dev/null
    "$CLI" decrypt -k "$KEY" -i "$TMP_DIR/2mb_${m}.enc" -o "$TMP_DIR/2mb_${m}.dec" >/dev/null
    cmp -s "$MB2" "$TMP_DIR/2mb_${m}.dec"
    pass "Multi-megabyte (2 MiB) round-trip in $m mode"
done

echo "=== 2. Benchmark (16 MiB Random Binary) ==="
BENCH16="$TMP_DIR/16mb.bin"
dd if=/dev/urandom of="$BENCH16" bs=1048576 count=16 2>/dev/null
for m in ctr cbc; do
    ct="$TMP_DIR/bench_${m}.enc"
    dt="$TMP_DIR/bench_${m}.dec"
    python3 - "$CLI" "$m" "$KEY" "$BENCH16" "$ct" "$dt" << 'EOF'
import sys, subprocess, time
cli, mode, key, src, ct, dt = sys.argv[1:7]
t0 = time.time()
subprocess.run([cli, "encrypt", "-m", mode, "-k", key, "-i", src, "-o", ct], check=True, stdout=subprocess.DEVNULL)
t1 = time.time()
subprocess.run([cli, "decrypt", "-k", key, "-i", ct, "-o", dt], check=True, stdout=subprocess.DEVNULL)
t2 = time.time()
enc_t, dec_t = t1 - t0, t2 - t1
mb = 16.0
print(f"  Benchmark 16 MiB [{mode.upper()}]: Encrypt {enc_t:.3f}s ({mb/enc_t:.2f} MB/s) | Decrypt {dec_t:.3f}s ({mb/dec_t:.2f} MB/s)")
EOF
    cmp -s "$BENCH16" "$dt"
    pass "Benchmark 16 MiB round-trip verified in $m mode"
done

echo "=== 3. Negative Tests & Tamper Resistance ==="
SAMPLE="$TMP_DIR/sample.bin"
dd if=/dev/urandom of="$SAMPLE" bs=1 count=64 2>/dev/null

for m in "${MODES[@]}"; do
    ct="$TMP_DIR/tamper_${m}.enc"
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$SAMPLE" -o "$ct" >/dev/null
    sz=$(wc -c < "$ct" | tr -d ' ')

    # Tamper Magic (offset 0..3) -> Exit code 3
    cp "$ct" "$TMP_DIR/t_magic.enc"
    flip_bit "$TMP_DIR/t_magic.enc" 0
    assert_decrypt_fail "$KEY" "$TMP_DIR/t_magic.enc" "3" "1-bit tamper on Magic byte 0 in $m mode"

    # Tamper IV (offset 6..21) -> Exit code 4
    cp "$ct" "$TMP_DIR/t_iv.enc"
    flip_bit "$TMP_DIR/t_iv.enc" 6
    assert_decrypt_fail "$KEY" "$TMP_DIR/t_iv.enc" "4" "1-bit tamper on IV byte 6 in $m mode"

    # Tamper Ciphertext (offset 22) -> Exit code 4
    cp "$ct" "$TMP_DIR/t_ct.enc"
    flip_bit "$TMP_DIR/t_ct.enc" 22
    assert_decrypt_fail "$KEY" "$TMP_DIR/t_ct.enc" "4" "1-bit tamper on Ciphertext byte 22 in $m mode"

    # Tamper Tag (last byte) -> Exit code 4
    cp "$ct" "$TMP_DIR/t_tag.enc"
    flip_bit "$TMP_DIR/t_tag.enc" $(( sz - 1 ))
    assert_decrypt_fail "$KEY" "$TMP_DIR/t_tag.enc" "4" "1-bit tamper on CMAC Tag in $m mode"

    # Wrong Key -> Exit code 4
    assert_decrypt_fail "$WRONG_KEY" "$ct" "4" "Wrong key rejection in $m mode"
done

echo "=== 4. Truncation and Foreign Format Rejection ==="
ct_sample="$TMP_DIR/tamper_ctr.enc"
sz=$(wc -c < "$ct_sample" | tr -d ' ')

# Cut last byte -> Exit code 4 or 3
cp "$ct_sample" "$TMP_DIR/t_cut1.enc"
dd if="$ct_sample" of="$TMP_DIR/t_cut1.enc" bs=1 count=$(( sz - 1 )) 2>/dev/null
assert_decrypt_fail "$KEY" "$TMP_DIR/t_cut1.enc" "3 4" "Truncated container (cut last byte)"

# Truncate to 37 bytes (< 38 min size) -> Exit code 3
dd if="$ct_sample" of="$TMP_DIR/t_cut37.enc" bs=1 count=37 2>/dev/null
assert_decrypt_fail "$KEY" "$TMP_DIR/t_cut37.enc" "3" "Truncated container (37 bytes < min container size)"

# 0-byte file -> Exit code 3
touch "$TMP_DIR/zero.bin"
assert_decrypt_fail "$KEY" "$TMP_DIR/zero.bin" "3" "Truncated container (0 bytes)"

# Foreign format (100 random bytes) -> Exit code 3
FOREIGN="$TMP_DIR/foreign.bin"
dd if=/dev/urandom of="$FOREIGN" bs=100 count=1 2>/dev/null
assert_decrypt_fail "$KEY" "$FOREIGN" "3" "Foreign format rejection (random 100 bytes)"

echo "=== 5. Cryptographic Properties (Freshness & Pattern Weakness) ==="

# IV Freshness (non-ECB modes: CBC, CFB, OFB, CTR)
for m in cbc cfb ofb ctr; do
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$SAMPLE" -o "$TMP_DIR/fresh1_${m}.enc" >/dev/null
    "$CLI" encrypt -m "$m" -k "$KEY" -i "$SAMPLE" -o "$TMP_DIR/fresh2_${m}.enc" >/dev/null
    if cmp -s "$TMP_DIR/fresh1_${m}.enc" "$TMP_DIR/fresh2_${m}.enc"; then
        echo "[FAIL] IV freshness violated: identical ciphertexts produced in $m mode" >&2
        exit 1
    fi
    pass "IV freshness verified in $m mode (unique ciphertext per run)"
done

# ECB Pattern Demonstration (identical plaintext blocks yield identical ciphertext blocks)
printf 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' > "$TMP_DIR/ecb_plain.bin" # 32 bytes = two 16B blocks
"$CLI" encrypt -m ecb -k "$KEY" -i "$TMP_DIR/ecb_plain.bin" -o "$TMP_DIR/ecb_pat.enc" >/dev/null
# Container header = 22 bytes. Ciphertext block 0 is at offset 22..37, block 1 is at offset 38..53.
dd if="$TMP_DIR/ecb_pat.enc" bs=1 skip=22 count=16 of="$TMP_DIR/ecb_b0.bin" 2>/dev/null
dd if="$TMP_DIR/ecb_pat.enc" bs=1 skip=38 count=16 of="$TMP_DIR/ecb_b1.bin" 2>/dev/null
if ! cmp -s "$TMP_DIR/ecb_b0.bin" "$TMP_DIR/ecb_b1.bin"; then
    echo "[FAIL] ECB pattern demonstration failed: identical blocks did not yield identical ciphertexts" >&2
    exit 1
fi
pass "ECB pattern weakness demonstrated: identical blocks produce identical ciphertext blocks"

# Contrast with CBC mode on identical blocks
"$CLI" encrypt -m cbc -k "$KEY" -i "$TMP_DIR/ecb_plain.bin" -o "$TMP_DIR/cbc_pat.enc" >/dev/null
dd if="$TMP_DIR/cbc_pat.enc" bs=1 skip=22 count=16 of="$TMP_DIR/cbc_b0.bin" 2>/dev/null
dd if="$TMP_DIR/cbc_pat.enc" bs=1 skip=38 count=16 of="$TMP_DIR/cbc_b1.bin" 2>/dev/null
if cmp -s "$TMP_DIR/cbc_b0.bin" "$TMP_DIR/cbc_b1.bin"; then
    echo "[FAIL] CBC unexpectedly produced identical ciphertext blocks for identical plaintext blocks" >&2
    exit 1
fi
pass "CBC pattern resistance verified: identical blocks produce distinct ciphertext blocks"

echo "=== All E2E tests passed cleanly ($PASS_COUNT checks) ==="
