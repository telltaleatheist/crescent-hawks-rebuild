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

# Every case runs on from the one before it, in one run of the original.
# "keys" are the original's keys and idle steps since the previous case
# (run.py's script language); chr is given the same input with the feeder's
# idle passes spelled out (chr_script()).
WHOLE_PANEL = (0, 0, 104, 200)
WHOLE_MAP = (104, 0, 216, 200)

CASES = [
    {
        # The first pass of the main loop is a game tick: 15 C-Bills (20 to
        # 35), the animated tiles on frame 1.
        "name": "town_start",
        "keys": "",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # 12 passes: the second game tick (the fence's frame 2).
        "name": "town_idle",
        "keys": "idle:9",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Each key is a game tick too; the fifth asks its question first.
        "name": "citadel_question",
        "keys": "up*5",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # No, then 250 more ticks: the 257th runs the money step again (35
        # to 50, the floor), and the fence is on frame 2.
        "name": "money_step",
        "keys": "n idle:2753",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
]

# The feeder (tools/capture/snap.asm, docs/reference.md) hands over a key at
# every 4th poll of the keyboard. The main loop polls once a pass, and after
# a key it flushes the keyboard with one more poll, so the town starts with
# 3 passes before the first item, and a key is followed by 2. idle:N is N.
FIRST_PASSES = 3
PASSES_AFTER_KEY = 2


def expand(keys):
    out = []
    for token in keys.replace(",", " ").split():
        name, _, count = token.partition("*")
        out += [name] * (int(count) if count else 1)
    return out


def chr_script(upto):
    """The chr script for the case at index `upto`: every item so far."""
    items = ["idle:%d" % FIRST_PASSES]
    for case in CASES[:upto + 1]:
        for k in expand(case["keys"]):
            items.append(k)
            if not k.startswith("idle:"):
                items.append("idle:%d" % PASSES_AFTER_KEY)
    return " ".join(items)


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
    finished, _ = run.run(a.game_dir, original_script(), ref_dir, timeout=240)
    if not finished:
        print("the original did not reach the end of the script")
        return 1

    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    ok = True
    for i, case in enumerate(CASES):
        name = case["name"]
        shot = os.path.join(a.work, name + ".bmp")
        subprocess.run([a.chr, "--game-dir", a.game_dir, "--data-dir", a.data_dir, "--map", "1",
                        "--script", chr_script(i), "--screenshot", shot],
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
        for known in case.get("known", []):
            print(f"  known: {known}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
