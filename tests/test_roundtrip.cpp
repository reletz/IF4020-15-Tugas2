// tests/test_roundtrip.cpp
#include <cstdio>
#include <cstring>
#include "cipher.hpp"

int main() {
  uint8_t key[KEY_SIZE] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  uint8_t ct[BLOCK_SIZE], back[BLOCK_SIZE], pt[BLOCK_SIZE] = {'h', 'e', 'l', 'l', 'o'};

  CustomCipher c(key);
  c.encrypt_block(pt, ct);
  c.decrypt_block(ct, back);

  bool changed = memcmp(pt, ct, BLOCK_SIZE) != 0;
  bool ok = changed && memcmp(pt, back, BLOCK_SIZE) == 0;
  printf(ok ? "PASS\n" : "FAIL\n");
  return ok ? 0 : 1;
}