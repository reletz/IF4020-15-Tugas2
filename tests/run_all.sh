#!/usr/bin/env bash
set -euo pipefail

# Jalankan semua tes dan analisis, simpan hasilnya ke $OUT (default test-results).
# Env: SKIP_TESTS=1 lewati unit test dan e2e; ANALYSIS_ARGS diteruskan ke mode_analysis.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
OUT="${OUT:-test-results}"
CLI=build/cipher_cli
KEY=000102030405060708090a0b0c0d0e0f
IV=0f0e0d0c0b0a09080706050403020100
MODES=(ecb cbc cfb ofb ctr)

mkdir -p "$OUT"/logs "$OUT"/data "$OUT"/plots "$OUT"/cli "$OUT"/samples

step() { echo; echo "=== $1"; }

if [ "${SKIP_TESTS:-0}" != "1" ]; then
    step "Unit test (make test) -> $OUT/logs/unit.log"
    make --no-print-directory test 2>&1 | tee "$OUT/logs/unit.log"

    step "End-to-end CLI (tests/e2e.sh) -> $OUT/logs/e2e.log"
    CLI="$CLI" bash tests/e2e.sh 2>&1 | tee "$OUT/logs/e2e.log"
fi

step "Avalanche 1 blok -> $OUT/logs/avalanche.log"
build/avalanche | tee "$OUT/logs/avalanche.log"

step "SAC 1 blok -> $OUT/logs/sac.log"
build/sac | tee "$OUT/logs/sac.log"

step "Analisis S-box -> $OUT/logs/sbox.log"
build/analyze_sbox | tee "$OUT/logs/sbox.log"

step "Avalanche per jumlah round -> $OUT/logs/rounds.log, $OUT/data/avalanche_rounds.csv"
build/rounds --csv "$OUT/data/avalanche_rounds.csv" | tee "$OUT/logs/rounds.log"

step "Analisis 5 mode (padding, avalanche, entropi, histogram) -> $OUT/logs/modes.log, $OUT/data/"
# shellcheck disable=SC2086
build/mode_analysis --out "$OUT/data" ${ANALYSIS_ARGS:-} | tee "$OUT/logs/modes.log"

step "Grafik histogram -> $OUT/plots/"
python3 analysis/plot_histogram.py "$OUT/data" "$OUT/plots"

step "Demo gambar ECB vs mode lain -> $OUT/ecb_image/"
python3 analysis/ecb_image.py "$CLI" "$OUT"

step "Round-trip CLI berkas uji (teks dan biner) x 5 mode -> $OUT/cli/"
# Kunci dan IV tetap supaya berkas .enc bisa direproduksi.
LOG="$OUT/cli/roundtrip.md"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
{
    echo "# Round-trip CLI berkas uji"
    echo
    echo "Kunci: \`$KEY\`, IV/counter awal: \`$IV\` (ECB tanpa IV)."
    echo "Ukuran kontainer = 22 (header) + payload + 16 (tag CMAC)."
    echo
    echo "| Berkas | Mode | Plaintext (B) | Kontainer (B) | Payload (B) | Dekripsi identik | SHA-256 plaintext (16 hex awal) |"
    echo "|---|---|---|---|---|---|---|"
} > "$LOG"
for f in "$OUT/samples/teks.txt" "$OUT/samples/gambar.png"; do
    name=$(basename "$f")
    n=$(wc -c < "$f" | tr -d ' ')
    sha=$(sha256sum "$f" | cut -c1-16)
    for m in "${MODES[@]}"; do
        enc="$OUT/cli/${name}.${m}.enc"
        if [ "$m" = "ecb" ]; then
            "$CLI" encrypt -f -m "$m" -k "$KEY" -i "$f" -o "$enc" >/dev/null
        else
            "$CLI" encrypt -f -m "$m" -k "$KEY" --iv "$IV" -i "$f" -o "$enc" >/dev/null
        fi
        "$CLI" decrypt -f -k "$KEY" -i "$enc" -o "$TMP/dec" >/dev/null
        size=$(wc -c < "$enc" | tr -d ' ')
        if cmp -s "$f" "$TMP/dec"; then ok="ya"; else ok="**TIDAK**"; fi
        echo "| $name | ${m^^} | $n | $size | $(( size - 38 )) | $ok | \`$sha\` |" >> "$LOG"
    done
done

# Data yang diubah, kunci salah, dan berkas terpotong harus ditolak
{
    echo
    echo "## Verifikasi integritas"
    echo
    echo "| Skenario | Exit code | Berkas output dibuat |"
    echo "|---|---|---|"
} >> "$LOG"
base="$OUT/cli/teks.txt.cbc.enc"
cp "$base" "$TMP/tamper.enc"
python3 -c "import sys;p=sys.argv[1];b=bytearray(open(p,'rb').read());b[30]^=1;open(p,'wb').write(b)" "$TMP/tamper.enc"
check_fail() {
    local label="$1" key="$2" file="$3"
    rm -f "$TMP/out"
    set +e
    "$CLI" decrypt -k "$key" -i "$file" -o "$TMP/out" >/dev/null 2>&1
    local rc=$?
    set -e
    local made="tidak"; [ -f "$TMP/out" ] && made="**ya**"
    echo "| $label | $rc | $made |" >> "$LOG"
}
check_fail "1 bit ciphertext dibalik (byte 30, CBC)" "$KEY" "$TMP/tamper.enc"
check_fail "Kunci salah (CBC)" "ffffffffffffffffffffffffffffffff" "$base"
head -c 37 "$base" > "$TMP/short.enc"
check_fail "Kontainer terpotong (37 B)" "$KEY" "$TMP/short.enc"
cat "$LOG"

echo
echo "=== Selesai. Hasil tersimpan di $OUT/"
