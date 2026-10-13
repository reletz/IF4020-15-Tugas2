#!/usr/bin/env python3
"""Grafik histogram SVG (plaintext dan 5 mode) dari hasil mode_analysis.

python3 analysis/plot_histogram.py <dir_data> <dir_output>
"""
import csv
import glob
import os
import sys

PANEL_W, PANEL_H = 330, 190
PLOT_X, PLOT_Y, PLOT_W, PLOT_H = 48, 34, 270, 120
COLS = 3


def read_entropy(path):
    info = {}
    if not os.path.exists(path):
        return info
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            info[(row["input"], row["mode"])] = row
    return info


def fmt_count(v):
    return f"{v / 1000:.1f}k" if v >= 1000 else str(int(v))


def panel(name, counts, x0, y0, note):
    total = sum(counts)
    expected = total / 256 if total else 0
    top = max(max(counts), expected) * 1.1 or 1
    color = "#7a7a7a" if name == "PLAIN" else "#2b6cb0"
    bar_w = PLOT_W / 256
    out = [f'<g transform="translate({x0},{y0})">',
           f'<text x="{PLOT_X}" y="16" class="t">{name}</text>',
           f'<text x="{PLOT_X}" y="28" class="n">{note}</text>',
           f'<rect x="{PLOT_X}" y="{PLOT_Y}" width="{PLOT_W}" height="{PLOT_H}" class="frame"/>']
    for b, c in enumerate(counts):
        h = PLOT_H * c / top
        if h > 0:
            out.append(f'<rect x="{PLOT_X + b * bar_w:.2f}" y="{PLOT_Y + PLOT_H - h:.2f}" '
                       f'width="{bar_w:.2f}" height="{h:.2f}" fill="{color}"/>')
    ey = PLOT_Y + PLOT_H - PLOT_H * expected / top
    out.append(f'<line x1="{PLOT_X}" x2="{PLOT_X + PLOT_W}" y1="{ey:.2f}" y2="{ey:.2f}" class="exp"/>')
    for frac in (0, 0.5, 1):
        v = top * frac
        y = PLOT_Y + PLOT_H - PLOT_H * frac
        out.append(f'<text x="{PLOT_X - 4}" y="{y + 3:.1f}" class="a" text-anchor="end">{fmt_count(v)}</text>')
    for b in (0, 64, 128, 192, 255):
        x = PLOT_X + b * bar_w
        out.append(f'<text x="{x:.1f}" y="{PLOT_Y + PLOT_H + 12}" class="a" text-anchor="middle">{b}</text>')
    out.append(f'<text x="{PLOT_X + PLOT_W / 2}" y="{PLOT_Y + PLOT_H + 26}" class="a" '
               f'text-anchor="middle">nilai byte</text>')
    out.append("</g>")
    return out


def plot(csv_path, entropy, out_dir):
    kind = os.path.basename(csv_path)[len("hist_"):-len(".csv")]
    with open(csv_path, newline="") as f:
        rows = list(csv.reader(f))
    header, data = rows[0], rows[1:]
    series = {name: [int(r[i]) for r in data] for i, name in enumerate(header) if i > 0}

    names = list(series)
    n_rows = (len(names) + COLS - 1) // COLS
    width, height = PANEL_W * COLS, PANEL_H * n_rows + 40
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" font-family="Helvetica, Arial, sans-serif">',
           "<style>.t{font-size:13px;font-weight:bold}.n{font-size:10px;fill:#444}"
           ".a{font-size:9px;fill:#555}.frame{fill:none;stroke:#bbb}"
           ".exp{stroke:#d9534f;stroke-dasharray:4 3;stroke-width:1}</style>",
           f'<rect width="{width}" height="{height}" fill="#fff"/>',
           f'<text x="16" y="24" font-size="15" font-weight="bold">Histogram frekuensi byte, input "{kind}"'
           f' (garis merah putus-putus = frekuensi seragam n/256)</text>']
    for i, name in enumerate(names):
        row = entropy.get((kind, name))
        note = ""
        if row:
            note = (f"H = {float(row['entropi_rata2']):.4f} bit/byte, "
                    f"chi2 = {float(row['chi2_rata2']):.1f}, n = {row['byte']}")
        svg += panel(name, series[name], (i % COLS) * PANEL_W, 40 + (i // COLS) * PANEL_H, note)
    svg.append("</svg>")
    out = os.path.join(out_dir, f"hist_{kind}.svg")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"  {out}")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    data_dir, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    entropy = read_entropy(os.path.join(data_dir, "entropy_modes.csv"))
    files = sorted(glob.glob(os.path.join(data_dir, "hist_*.csv")))
    if not files:
        sys.exit(f"Tidak ada hist_*.csv di {data_dir}. Jalankan 'make mode-analysis' dulu.")
    for path in files:
        plot(path, entropy, out_dir)


if __name__ == "__main__":
    main()
