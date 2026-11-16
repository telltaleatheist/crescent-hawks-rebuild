#!/usr/bin/env python3
"""Compare the rebuild with the original, screen by screen.

    check.py --chr BUILD/chr --game-dir DIR --data-dir DIR --work DIR

Runs the original headless once (run.py) through the cases below, renders
the same steps with `chr --script ... --screenshot`, and compares them pixel
for pixel (compare.py). Each case lists the rectangles the rebuild already
draws, which must be identical, and the differences still known, with their
cause in the original. A rectangle that stops matching fails the check; the
known differences are only reported, so that each one is fixed on purpose.

Exit status: 0 all required rectangles match, 1 a mismatch, 77 skipped
(no DOSBox-X, nasm or game folder: the check needs all three).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare  # noqa: E402
import run  # noqa: E402

SKIP = 77

# The original's keys to reach each screen. The start-up asks for the
# graphics adapter (3: 256K EGA/VGA) and the disk drives (3: hard disk), then
# waits on a note, the Infocom screen and the title, then asks "Is this your
# first time playing BattleTech?" (Enter: yes) and starts the town.
START = "3 3 enter enter enter enter"

CASES = [
    {
        "name": "town_start",
        "chr_script": "",
        "match": {
            "map above the party": (104, 0, 216, 96),
            "panel: upper box and frames": (0, 0, 104, 112),
            "panel: party names": (0, 112, 72, 64),
        },
        "known": [
            "the party's figure (1000:051b, not drawn yet)",
            "animated tiles, e.g. the fence (1000:240b: a 3-frame cycle from ANIMATE.ICN, advanced "
            "every 11 idle ticks of the main loop 1000:01f4)",
            "the panel's three bars per character (1000:4bc1)",
            "C-Bills and their amount (1e31:1fdf; the amount grows in the main loop's game tick)",
        ],
    },
    {
        "name": "citadel_question",
        "chr_script": "up*5",
        "keys": "up*5",
        "match": {
            "the question box": (104, 72, 216, 56),
            "map above the box": (104, 0, 216, 72),
            "panel: the move log and frames": (0, 0, 104, 112),
            "panel: party names": (0, 112, 72, 64),
        },
        "known": [
            "animated tiles below the box (1000:240b)",
            "the panel's bars and C-Bills (1000:4bc1, 1e31:1fdf)",
        ],
    },
]


def original_script():
    tokens = [START]
    for case in CASES:
        if case.get("keys"):
            tokens.append(case["keys"])
        tokens.append("@snap:" + case["name"])
    return " ".join(tokens)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--chr", required=True)
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--work", required=True, help="where the captures, screenshots and diffs go")
    a = ap.parse_args()

    missing = [what for what, ok in (("DOSBox-X", os.path.exists(run.DOSBOX)),
                                     ("nasm", shutil.which("nasm")),
                                     ("the game folder", os.path.exists(os.path.join(a.game_dir, "BTECH.EXE"))))
               if not ok]
    if missing:
        print("skipped: missing " + ", ".join(missing))
        return SKIP

    os.makedirs(a.work, exist_ok=True)
    ref_dir = os.path.join(a.work, "original")
    finished, _ = run.run(a.game_dir, original_script(), ref_dir, timeout=120)
    if not finished:
        print("the original did not reach the end of the script")
        return 1

    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    ok = True
    for case in CASES:
        name = case["name"]
        shot = os.path.join(a.work, name + ".bmp")
        subprocess.run([a.chr, "--game-dir", a.game_dir, "--data-dir", a.data_dir, "--map", "1",
                        "--script", case["chr_script"], "--screenshot", shot],
                       check=True, env=env, stdout=subprocess.DEVNULL)
        ref = compare.reference_pixels(os.path.join(ref_dir, name + ".bin"))
        ours = compare.bmp_pixels(shot)
        compare.write_diff(os.path.join(a.work, name + "-diff.png"), ref, ours)
        print(f"== {name}")
        for region in ("panel", "play"):
            n = len(compare.compare(ref, ours, region))
            print(f"  {region}: {n} pixels differ")
        for label, rect in case["match"].items():
            n = len(compare.compare_rect(ref, ours, rect))
            print(f"  {'ok  ' if n == 0 else 'FAIL'} {label}: {n} pixels differ")
            ok = ok and n == 0
        for known in case["known"]:
            print(f"  known: {known}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
