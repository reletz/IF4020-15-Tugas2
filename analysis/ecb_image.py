#!/usr/bin/env python3
"""Demo pola ECB: piksel gambar grayscale dienkripsi di 5 mode lalu ditampilkan ulang.

python3 analysis/ecb_image.py <path_cipher_cli> <dir_output>
-> <dir_keluaran>/samples/gambar.png dan <dir_output>/ecb_image/<mode>.png
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib

W = H = 256
KEY = "000102030405060708090a0b0c0d0e0f"
IV = "0f0e0d0c0b0a09080706050403020100"
HEADER = 22  # magic 4 + versi 1 + mode 1 + IV 16


def write_png(path, pixels):
    """Tulis PNG grayscale 8-bit W x H dari bytes piksel."""
    raw = b"".join(b"\x00" + pixels[y * W:(y + 1) * W] for y in range(H))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def make_image():
    """Gambar uji: latar terang, lingkaran gelap, persegi abu-abu, segitiga."""
    px = bytearray([235] * (W * H))
    for y in range(H):
        for x in range(W):
            if (x - 88) ** 2 + (y - 92) ** 2 <= 62 ** 2:
                px[y * W + x] = 40
            elif 150 <= x <= 236 and 30 <= y <= 130:
                px[y * W + x] = 140
            elif 150 <= y <= 240 and abs(x - 128) <= (y - 150) * 98 / 90:
                px[y * W + x] = 90
    return bytes(px)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    cli, out_dir = sys.argv[1], sys.argv[2]
    img_dir = os.path.join(out_dir, "ecb_image")
    sample_dir = os.path.join(out_dir, "samples")
    os.makedirs(img_dir, exist_ok=True)
    os.makedirs(sample_dir, exist_ok=True)

    pixels = make_image()
    write_png(os.path.join(sample_dir, "gambar.png"), pixels)
    write_png(os.path.join(img_dir, "plain.png"), pixels)
    print(f"  {os.path.join(img_dir, 'plain.png')}")

    with tempfile.TemporaryDirectory() as tmp:
        raw_path = os.path.join(tmp, "pixels.raw")
        with open(raw_path, "wb") as f:
            f.write(pixels)
        for mode in ("ecb", "cbc", "cfb", "ofb", "ctr"):
            enc_path = os.path.join(tmp, f"{mode}.enc")
            cmd = [cli, "encrypt", "-f", "-m", mode, "-k", KEY, "-i", raw_path, "-o", enc_path]
            if mode != "ecb":
                cmd[5:5] = ["--iv", IV]
            subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
            with open(enc_path, "rb") as f:
                body = f.read()[HEADER:HEADER + len(pixels)]
            out = os.path.join(img_dir, f"{mode}.png")
            write_png(out, body)
            print(f"  {out}")


if __name__ == "__main__":
    main()
