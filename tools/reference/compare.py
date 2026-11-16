#!/usr/bin/env python3
"""Compare a capture of the original (SNAP .bin) with a chr screenshot (.bmp).

    compare.py REFERENCE.bin CHR.bmp [--diff OUT.png] [--region NAME]

Both are brought to the original's 320x200 screen: the reference through
the attribute palette and DAC it recorded, the chr screenshot by sampling
one pixel per screen pixel (chr draws at a whole-number scale). Pixels are
compared as colours, so the two need not use the same palette numbers.

Regions (the screen layout, docs/text.md): panel = x 0-103, play = x 104-319,
all = the whole screen. Prints, per region, how many pixels differ, and the
bounding box of the differences. Exit status 0 if the chosen regions match.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "capture"))
import snap2png  # noqa: E402

REGIONS = {"panel": (0, 0, 104, 200), "play": (104, 0, 216, 200), "all": (0, 0, 320, 200)}


def reference_pixels(path):
    w, h, rows = snap2png.decode(open(path, "rb").read())
    return [[tuple(r[3 * x:3 * x + 3]) for x in range(w)] for r in rows]


def bmp_pixels(path):
    """A 24- or 32-bit uncompressed (or BITFIELDS) BMP, sampled down to 320x200."""
    d = open(path, "rb").read()
    if d[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP")
    offset = struct.unpack_from("<I", d, 10)[0]
    width, height = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    compression = struct.unpack_from("<I", d, 30)[0]
    if bpp not in (24, 32) or compression not in (0, 3):
        raise ValueError(f"{path}: {bpp}-bit BMP, compression {compression}, not handled")
    masks = None
    if compression == 3:
        masks = struct.unpack_from("<III", d, 54)
    bottom_up = height > 0
    height = abs(height)
    stride = (width * bpp // 8 + 3) & ~3
    if width % 320 or height % 200 or width // 320 != height // 200:
        raise ValueError(f"{path}: {width}x{height} is not a whole-number scale of 320x200")
    scale = width // 320

    def pixel(x, y):
        row = height - 1 - y if bottom_up else y
        o = offset + row * stride + x * (bpp // 8)
        if bpp == 24:
            b, g, r = d[o:o + 3]
            return (r, g, b)
        v = struct.unpack_from("<I", d, o)[0]
        if masks:
            out = []
            for m in masks:
                shift = (m & -m).bit_length() - 1
                out.append((v & m) >> shift)
            return tuple(out)
        return ((v >> 16) & 255, (v >> 8) & 255, v & 255)

    return [[pixel(x * scale + scale // 2, y * scale + scale // 2) for x in range(320)] for y in range(200)]


def compare(ref, ours, region):
    return compare_rect(ref, ours, REGIONS[region])


def compare_rect(ref, ours, rect):
    """The (x, y) of every differing pixel in the rectangle (x, y, width, height)."""
    x0, y0, w, h = rect
    diff = []
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            if ref[y][x] != ours[y][x]:
                diff.append((x, y))
    return diff


def write_diff(path, ref, ours):
    """Left: the original. Middle: chr. Right: the differences in magenta over a dimmed original."""
    rows = []
    for y in range(200):
        row = []
        for x in range(320):
            row.extend(ref[y][x])
        row.extend((255, 0, 255) * 2)
        for x in range(320):
            row.extend(ours[y][x])
        row.extend((255, 0, 255) * 2)
        for x in range(320):
            if ref[y][x] != ours[y][x]:
                row.extend((255, 0, 255))
            else:
                row.extend(c // 3 for c in ref[y][x])
        rows.append(row)
    snap2png.png(path, 964, 200, rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference")
    ap.add_argument("ours")
    ap.add_argument("--diff")
    ap.add_argument("--region", action="append", choices=sorted(REGIONS))
    a = ap.parse_args()
    ref = reference_pixels(a.reference)
    ours = bmp_pixels(a.ours)
    if a.diff:
        write_diff(a.diff, ref, ours)
    ok = True
    for region in a.region or ["panel", "play"]:
        diff = compare(ref, ours, region)
        x0, y0, w, h = REGIONS[region]
        if diff:
            ok = False
            xs = [p[0] for p in diff]
            ys = [p[1] for p in diff]
            print(f"{region}: {len(diff)} of {w * h} pixels differ, in x {min(xs)}-{max(xs)}, y {min(ys)}-{max(ys)}")
        else:
            print(f"{region}: identical")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
