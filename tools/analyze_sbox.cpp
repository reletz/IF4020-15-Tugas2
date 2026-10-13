// Analisis S-box: bijektif, fixed point, nonlinearity, differential uniformity,
// derajat aljabar, dan SAC. Jalankan dengan: make analyze

#include <cstdio>
#include <cstring>
#include "sbox.hpp"

static int parity(unsigned v) { return __builtin_popcount(v) & 1; }

int main() {
    // 1. Bijektif, fixed point
    int seen[256] = {0}, fixed = 0, opp_fixed = 0;
    bool bijective = true;
    for (int x = 0; x < 256; x++) {
        if (seen[SBOX[x]]++) bijective = false;
        if (SBOX[x] == x) fixed++;
        if (SBOX[x] == (uint8_t)~x) opp_fixed++;
    }

    // 2. Nonlinearity (min atas 255 fungsi komponen b . S(x))
    int min_nl = 128, max_walsh = 0;
    static int W[256];
    for (int b = 1; b < 256; b++) {
        for (int x = 0; x < 256; x++) W[x] = parity((unsigned)(b & SBOX[x])) ? -1 : 1;
        for (int step = 1; step < 256; step <<= 1)
            for (int j = 0; j < 256; j += step << 1)
                for (int k = j; k < j + step; k++) {
                    int u = W[k], v = W[k + step];
                    W[k] = u + v;
                    W[k + step] = u - v;
                }
        int mw = 0;
        for (int a = 0; a < 256; a++) {
            int w = W[a] < 0 ? -W[a] : W[a];
            if (w > mw) mw = w;
        }
        if (mw > max_walsh) max_walsh = mw;
        int nl = 128 - mw / 2;
        if (nl < min_nl) min_nl = nl;
    }

    // 3. Differential uniformity
    static int ddt[256][256];
    memset(ddt, 0, sizeof(ddt));
    for (int x = 0; x < 256; x++)
        for (int dx = 1; dx < 256; dx++)
            ddt[dx][SBOX[x] ^ SBOX[x ^ dx]]++;
    int du = 0, du_count = 0;
    for (int dx = 1; dx < 256; dx++)
        for (int dy = 0; dy < 256; dy++) {
            if (ddt[dx][dy] > du) { du = ddt[dx][dy]; du_count = 0; }
            if (ddt[dx][dy] == du) du_count++;
        }

    // 4. Derajat aljabar fungsi komponen (ANF lewat transformasi Mobius)
    int deg_min = 8, deg_max = 0;
    for (int b = 1; b < 256; b++) {
        int a[256];
        for (int x = 0; x < 256; x++) a[x] = parity((unsigned)(b & SBOX[x]));
        for (int i = 0; i < 8; i++)
            for (int x = 0; x < 256; x++)
                if (x & (1 << i)) a[x] ^= a[x ^ (1 << i)];
        int deg = 0;
        for (int x = 0; x < 256; x++)
            if (a[x] && __builtin_popcount((unsigned)x) > deg) deg = __builtin_popcount((unsigned)x);
        if (deg < deg_min) deg_min = deg;
        if (deg > deg_max) deg_max = deg;
    }

    // 5. SAC S-box
    double sac_sum = 0, sac_min = 1, sac_max = 0;
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            int cnt = 0;
            for (int x = 0; x < 256; x++)
                cnt += ((SBOX[x] ^ SBOX[x ^ (1 << i)]) >> j) & 1;
            double p = cnt / 256.0;
            sac_sum += p;
            if (p < sac_min) sac_min = p;
            if (p > sac_max) sac_max = p;
        }

    printf("Analisis S-box 8-bit (SBOX, src/core/sbox_table.inc)\n\n");
    printf("%-34s %-14s %s\n", "Metrik", "S-box ini", "Pembanding");
    printf("------------------------------------------------------------------------------\n");
    printf("%-34s %-14s %s\n", "Bijektif", bijective ? "ya" : "TIDAK", "wajib ya");
    printf("%-34s %-14d %s\n", "Fixed point S[x] = x", fixed, "ideal 0 (AES: 0)");
    printf("%-34s %-14d %s\n", "Opposite fixed point S[x] = ~x", opp_fixed, "ideal 0 (AES: 0)");
    printf("%-34s %-14d %s\n", "Nonlinearity (min komponen)", min_nl, "AES 112, acak tipikal 92-100");
    printf("%-34s %-14d %s\n", "Max |Walsh|", max_walsh, "AES 32 (makin kecil makin baik)");
    printf("%-34s %-14d %s\n", "Differential uniformity", du, "AES 4, acak tipikal 10-12");
    printf("%-34s %-14d %s\n", "Jumlah entri DDT bernilai maks", du_count, "makin sedikit makin baik");
    printf("%-34s %d-%-12d %s\n", "Derajat aljabar komponen (min-max)", deg_min, deg_max, "AES 7-7, maks 7");
    printf("%-34s %.3f (%.3f-%.3f) %s\n", "SAC rata2 (min-max)", sac_sum / 64, sac_min, sac_max, "ideal 0.500");
    printf("\nProbabilitas diferensial maks = %d/256 = %.4f, bias linear maks = %d/512 = %.4f\n", du,
           du / 256.0, max_walsh, max_walsh / 512.0);
    return bijective ? 0 : 1;
}
