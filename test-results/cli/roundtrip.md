# Round-trip CLI

Key: `000102030405060708090a0b0c0d0e0f`
IV/counter awal: `0f0e0d0c0b0a09080706050403020100` (ECB tanpa IV).
Ukuran container = 22 (header) + payload + 16 (tag CMAC).

| Berkas | Mode | Plaintext (B) | Container (B) | Payload (B) | Dekripsi identik? | SHA-256 plaintext (16 hex awal) |
|---|---|---|---|---|---|---|
| teks.txt | ECB | 1116 | 1158 | 1120 | ya | `0118caf99b62bdf9` |
| teks.txt | CBC | 1116 | 1158 | 1120 | ya | `0118caf99b62bdf9` |
| teks.txt | CFB | 1116 | 1154 | 1116 | ya | `0118caf99b62bdf9` |
| teks.txt | OFB | 1116 | 1154 | 1116 | ya | `0118caf99b62bdf9` |
| teks.txt | CTR | 1116 | 1154 | 1116 | ya | `0118caf99b62bdf9` |
| gambar.png | ECB | 949 | 998 | 960 | ya | `5acafa9454163dc6` |
| gambar.png | CBC | 949 | 998 | 960 | ya | `5acafa9454163dc6` |
| gambar.png | CFB | 949 | 987 | 949 | ya | `5acafa9454163dc6` |
| gambar.png | OFB | 949 | 987 | 949 | ya | `5acafa9454163dc6` |
| gambar.png | CTR | 949 | 987 | 949 | ya | `5acafa9454163dc6` |

## Verifikasi integritas

| Skenario | Exit code | Berkas output dibuat |
|---|---|---|
| 1 bit ciphertext dibalik (byte 30, CBC) | 4 | tidak |
| Kunci salah (CBC) | 4 | tidak |
| Kontainer terpotong (37 B) | 3 | tidak |
