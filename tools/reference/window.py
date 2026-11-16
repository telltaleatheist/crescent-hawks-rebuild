#!/usr/bin/env python3
"""Compares the rebuild's cell window with the original's (docs/wilderness.md).

For each capture given, reads where the original last built its cell window
(P:02cf, 02d1), the window's nine block values (P:07a4) and its cells
(P:07ad), asks `chr --window X,Y` for the window it builds at the same
place, and reports the captures that differ. The terrain must be the one a
new game loads (true of every sequence so far).

    window.py --chr build/chr --game-dir GAME --data-dir DATA CAPTURE.bin...
"""
import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from memory import Memory  # noqa: E402


def original_window(path):
    m = Memory(path)
    p = m.word(m.ds, 0x562a)  # 2c6c, the display library's data
    at = (m.word(p, 0x02cf), m.word(p, 0x02d1))
    lines = ["window %04x %04x blocks %s" % (at[0], at[1], m.read(p, 0x07a4, 9).hex())]
    for r in range(24):
        lines.append("window %04x %04x row %2d %s" % (at[0], at[1], r, m.read(p, 0x07ad + 24 * r, 24).hex()))
    # What follows the window and its corner words up to the block store
    # (docs/wilderness.md): the map-load queue, the noise cursor, the noise
    # table and the heights. The corner words are left out: P:09ef and 09f1
    # are wherever 287f:1df8 last ran, which the rebuild does not follow.
    lines.append("window %04x %04x after %s" % (at[0], at[1], m.read(p, 0x09f3, 0x0c1d - 0x09f3).hex()))
    return at, lines


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--chr", required=True)
    a.add_argument("--game-dir", required=True)
    a.add_argument("--data-dir", required=True)
    a.add_argument("captures", nargs="+")
    a = a.parse_args()
    wanted = {path: original_window(path) for path in a.captures}
    places = sorted({at for at, _ in wanted.values()})
    args = [a.chr, "--game-dir", a.game_dir, "--data-dir", a.data_dir, "--map", "1", "--script", "idle:0",
            "--screenshot", os.devnull]
    for x, y in places:
        args += ["--window", "%x,%x" % (x, y)]
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    out = subprocess.run(args, capture_output=True, text=True, env=env, check=True).stdout
    ours = [line for line in out.splitlines() if line.startswith("window ")]
    failed = 0
    for path, (at, lines) in wanted.items():
        key = "window %04x %04x " % at
        mine = [line for line in ours if line.startswith(key)]
        differ = sum(1 for o, c in zip(lines, mine) if o != c) + abs(len(lines) - len(mine))
        if differ:
            failed += 1
            print("FAIL %s at %04x %04x: %d lines differ" % (path, at[0], at[1], differ))
    print("%d captures, %d places, %d differ" % (len(wanted), len(places), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
