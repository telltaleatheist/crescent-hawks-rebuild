#!/usr/bin/env python3
"""Convert SNAP.COM captures (SNAPnnnn.BIN) to PNG.

Usage: snap2png.py SNAP0000.BIN [...] [-o OUTDIR] [--scale N]

EGA mode 0Dh: the four planes are combined into 4-bit pixels, each pixel goes
through the attribute palette register, and that value indexes the DAC
(6-bit RGB). Mode 13h: each byte indexes the DAC directly.
"""
import argparse
import os
import struct
import zlib


def png(path, w, h, rgb_rows):
    raw = b"".join(b"\0" + bytes(r) for r in rgb_rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def decode(data):
    if data[:4] != b"CHRS":
        raise ValueError("not a SNAP capture")
    mode = data[5]
    attr = data[8:24]
    dac = data[24:24 + 768]
    pic = data[24 + 768:]

    def rgb(i):
        r, g, b = dac[3 * i:3 * i + 3]
        return (r * 255 // 63, g * 255 // 63, b * 255 // 63)

    rows = []
    if mode == 0x0D:
        if len(pic) < 32000:
            raise ValueError("EGA capture is short")
        planes = [pic[p * 8000:(p + 1) * 8000] for p in range(4)]
        for y in range(200):
            row = []
            for xb in range(40):
                o = y * 40 + xb
                bits = [planes[p][o] for p in range(4)]
                for bit in range(7, -1, -1):
                    v = 0
                    for p in range(4):
                        v |= ((bits[p] >> bit) & 1) << p
                    row.extend(rgb(attr[v] & 0x3F))
            rows.append(row)
    elif mode == 0x13:
        for y in range(200):
            row = []
            for x in range(320):
                row.extend(rgb(pic[y * 320 + x]))
            rows.append(row)
    else:
        raise ValueError(f"video mode {mode:#x} not handled")
    return 320, 200, rows


def scale(w, h, rows, n):
    out = []
    for r in rows:
        line = []
        for x in range(w):
            line.extend(r[3 * x:3 * x + 3] * n)
        out.extend([line] * n)
    return w * n, h * n, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("-o", "--outdir", default=".")
    ap.add_argument("--scale", type=int, default=1)
    a = ap.parse_args()
    for f in a.files:
        data = open(f, "rb").read()
        if len(data) > 5 and data[5] <= 3:
            txt = data[24 + 768:24 + 768 + 4000]
            lines = ["".join(chr(txt[2 * (y * 80 + x)]) if 32 <= txt[2 * (y * 80 + x)] < 127 else " "
                             for x in range(80)).rstrip() for y in range(25)]
            out = os.path.join(a.outdir, os.path.splitext(os.path.basename(f))[0] + ".txt")
            open(out, "w").write("\n".join(lines) + "\n")
            print(out)
            continue
        try:
            w, h, rows = decode(data)
        except ValueError as e:
            print(f"{f}: {e} (mode {data[5]:#x})" if len(data) > 5 else f"{f}: {e}")
            continue
        if a.scale > 1:
            w, h, rows = scale(w, h, rows, a.scale)
        out = os.path.join(a.outdir, os.path.splitext(os.path.basename(f))[0] + ".png")
        png(out, w, h, rows)
        print(out)


if __name__ == "__main__":
    main()
