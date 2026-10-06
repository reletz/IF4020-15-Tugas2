#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>

static uint64_t xstate;

static inline uint64_t xorshift64() {
    xstate ^= xstate << 13;
    xstate ^= xstate >> 7;
    xstate ^= xstate << 17;
    return xstate;
}

static void fy_shuffle(uint8_t s[256]) {
    for (int i = 255; i > 0; i--) {
        // Modulo bias diabaikan: 2^64 mod (i+1) << 2^64, error < 0.01%
        int j = (int)(xorshift64() % (uint64_t)(i + 1));
        uint8_t tmp = s[i]; s[i] = s[j]; s[j] = tmp;
    }
}

static int fixed_points(const uint8_t s[256]) {
    int cnt = 0;
    for (int i = 0; i < 256; i++) if (s[i] == (uint8_t)i) cnt++;
    return cnt;
}

static int diff_uniformity(const uint8_t s[256]) {
    static int ddt[256][256];
    memset(ddt, 0, sizeof(ddt));
    for (int x = 0; x < 256; x++)
        for (int dx = 1; dx < 256; dx++)
            ddt[dx][s[x] ^ s[x ^ dx]]++;
    int max_val = 0;
    for (int dx = 1; dx < 256; dx++)
        for (int dy = 0; dy < 256; dy++)
            if (ddt[dx][dy] > max_val) max_val = ddt[dx][dy];
    return max_val;
}


static int compute_nl(const uint8_t s[256]) {
    static int W[256];
    int min_nl = 128;

    for (int b = 1; b < 256; b++) {
        // Inisialisasi: +1 jika parity genap, -1 jika ganjil
        for (int x = 0; x < 256; x++)
            W[x] = (__builtin_popcount((unsigned)(b & s[x])) & 1) ? -1 : 1;

        // Butterfly WHT in-place
        for (int step = 1; step < 256; step <<= 1) {
            for (int j = 0; j < 256; j += step << 1) {
                for (int k = j; k < j + step; k++) {
                    int u = W[k], v = W[k + step];
                    W[k]        = u + v;
                    W[k + step] = u - v;
                }
            }
        }

        // Cari max |W[a]|
        int max_w = 0;
        for (int a = 0; a < 256; a++) {
            int w = (W[a] < 0) ? -W[a] : W[a];
            if (w > max_w) max_w = w;
        }

        int nl = 128 - max_w / 2;
        if (nl < min_nl) min_nl = nl;
    }
    return min_nl;
}

static void print_sbox_inc(const uint8_t s[256], uint64_t seed, int iters) {
    printf("// auto generated oleh tools/gen_sbox.cpp\n");
    printf("// Seed PRNG (xorshift64) : %lluULL (digit pi)\n", (unsigned long long)seed);
    printf("// Iterasi Fisher-Yates : %d\n", iters);
    for (int i = 0; i < 256; i++) {
        if (i % 16 == 0) printf("    ");
        printf("0x%02X", s[i]);
        if (i < 255) printf(",");
        if (i % 16 == 15) printf("\n");
        else               printf(" ");
    }
}

int main() {
    static uint8_t best[256], cand[256];
    int best_nl = -1, best_du = INT_MAX;
    bool found = false;

    const int ITERS      = 10000;
    const uint64_t SEED     = 3141592653589793ULL;

    xstate = SEED;

    for (int iter = 0; iter < ITERS; iter++) {
        // Mulai dari identitas lalu acak
        for (int i = 0; i < 256; i++) cand[i] = (uint8_t)i;
        fy_shuffle(cand);

        // Syarat mutlak: tidak ada fixed point
        if (fixed_points(cand) != 0) continue;

        // Hitung differential uniformity dulu (lebih murah)
        int du = diff_uniformity(cand);

        // Pruning agresif: lewati kalau du sudah > best_du + 4
        if (found && du > best_du + 4) continue;

        // Hitung nonlinearity
        int nl = compute_nl(cand);

        bool better = !found
            || nl > best_nl
            || (nl == best_nl && du < best_du);

        if (better) {
            memcpy(best, cand, 256);
            best_nl = nl;
            best_du = du;
            found   = true;
        }
    }

    if (!found) {
        fprintf(stderr, "ERROR: tidak ada kandidat valid.\n");
        return 1;
    }

    print_sbox_inc(best, SEED, ITERS);
    return 0;
}