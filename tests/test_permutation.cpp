// tests/test_permutation.cpp
#include <cstdio>
#include <set>
#include "permutation.hpp"

int main() {
  std::set<uint64_t> seen;
  for (int i = 0; i < 64; i++) {
    uint64_t out = permute_bits(1ULL << i);
    // satu bit masuk -> tepat satu bit keluar
    if (out == 0 || (out & (out - 1)) != 0) {
      printf("FAIL: bit %d tidak menghasilkan tepat satu bit\n", i);
      return 1;
    }
    seen.insert(out);
  }
  // 64 bit masuk -> 64 posisi berbeda (bijektif)
  bool ok = seen.size() == 64;
  printf(ok ? "PASS\n" : "FAIL: ada posisi tujuan kembar\n");
  return ok ? 0 : 1;
}
