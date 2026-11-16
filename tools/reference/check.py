#!/usr/bin/env python3
"""Compare the rebuild with the original, screen by screen.

    check.py --chr BUILD/chr --game-dir DIR --data-dir DIR --work DIR [--full]

Runs the original headless once (run.py) through the cases below, renders
the same steps with `chr --script ... --screenshot`, and compares them pixel
for pixel (compare.py). Each case lists the rectangles the rebuild already
draws, which must be identical, and the differences still known, with their
cause in the original. A rectangle that stops matching fails the check; the
known differences are only reported, so that each one is fixed on purpose.
Each case also compares the game state (`chr --state` against the
original's memory, state.py): the random numbers, C-Bills and the
townspeople's records must be identical.

The quick cases run in ctest (`reference_compare`); --full adds the long
ones (thousands of passes of idling, a walk across MAP1), a few minutes.

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
import state  # noqa: E402

SKIP = 77

# The original's keys before the first case. The start-up asks in text mode
# for the graphics adapter (3: 256K EGA/VGA) and the disk drives (3: hard
# disk), then waits on a note about saved games. The rebuild leaves those to
# the platform; its start-up begins with the Infocom screen (the first case).
START = "3 3 enter"

# Every case runs on from the one before it, in one run of the original.
# "keys" are the original's keys and idle steps since the previous case
# (run.py's script language); chr is given the same input with the feeder's
# idle passes spelled out (chr_script()).
WHOLE_PANEL = (0, 0, 104, 200)
WHOLE_MAP = (104, 0, 216, 200)

CASES = [
    {
        # The start-up (docs/startup.md): the Infocom screen, up until a key
        # or 700 frames. No game state yet.
        "name": "start_infocom",
        "keys": "",
        "state": False,
        "match": {"the screen": (0, 0, 320, 200)},
    },
    {
        # The title, while its tune plays, until a key.
        "name": "start_title",
        "keys": "enter",
        "state": False,
        "match": {"the screen": (0, 0, 320, 200)},
    },
    {
        # A new game's panel, the notice and "Is this your first time
        # playing BattleTech?" in window 6, Yes picked.
        "name": "start_first_time",
        "keys": "enter",
        "state": False,
        "match": {"the panel": WHOLE_PANEL, "the window": WHOLE_MAP},
    },
    {
        # The first pass of the main loop is a game tick: 15 C-Bills (20 to
        # 35), the animated tiles on frame 1.
        "name": "town_start",
        "keys": "enter",
        "starts_town": True,
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # 12 passes: the second game tick (the fence's frame 2).
        "name": "town_idle",
        "keys": "idle:9",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # 120 more passes: the townsperson nearest the party (the fifth) walks
        # toward its next entrance, in view.
        "name": "people_walking",
        "keys": "idle:120",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Each key is a game tick too; the fifth asks its question first.
        "name": "citadel_question",
        "keys": "up*5",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Yes: the Citadel (CITADEL.BLD, docs/scripts.md): its picture in the
        # upper left box, its text and menu with the bar on the first item.
        "name": "citadel_inside",
        "keys": "y",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The first item: the sentry's text, a key, then Katrina's (with
        # flag 00 clear), which sets flags 16 and 06.
        "name": "citadel_katrina",
        "keys": "enter enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Back at the menu, the bar where it was left; down to the Hall of
        # Legends, its first exhibit.
        "name": "citadel_hall",
        "keys": "enter down enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
]

# Run after CASES with --full, continuing from where they end.
FULL_CASES = [
    {
        # The Hall's last exhibit.
        "name": "citadel_hall_end",
        "keys": "enter*5",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The combat class: the welcome (the first time, flag 07).
        "name": "citadel_class",
        "keys": "enter down enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The courses with their prices (special 04), menu 9.
        "name": "citadel_class_menu",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The first course: 75 C-Bills, and Jason has 35 (special 05).
        "name": "citadel_no_money",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Talk to others (special 12, 17dc:17b9): who is inside the Citadel
        # (nobody, at this point).
        "name": "citadel_talk",
        "keys": "enter down enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Nobody is inside at this point: a key goes back to the menu, the bar
        # on "talk" where it was left.
        "name": "citadel_talk_more",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Leave: special 14, the upper box cleared, the panel drawn again,
        # and the step goes on as after "no"; its game tick draws the map.
        "name": "citadel_leave",
        "keys": "down enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Out of the Citadel, then 250 more ticks: the 257th runs the money
        # step again (35 to 50, the floor), and the fence is on frame 2.
        "name": "money_step",
        "keys": "idle:2753",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # A walk west and south through MAP1: scrolling, fences, and the
        # townspeople's steps against where the party was last drawn.
        "name": "walk_west",
        "keys": "down*4 left*26 down*24 right*4 down*10",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        "name": "walk_idle",
        "keys": "idle:400",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Back, then east along the street, round the shops and west again.
        "name": "walk_east",
        "keys": "up*10 left*4 up*24 right*60 up*8 pgup*6 right*10 down*6 left*30 home*4 left*10",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Waiting by the Barracks door: the fourth townsperson reaches it and
        # goes in for a random number of rounds (24).
        "name": "door_in",
        "keys": "idle:600",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # ... and comes out again, for the next entrance.
        "name": "door_out",
        "keys": "idle:1200",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
]

# Training mission 2 (docs/combat.md), a separate run from a new game.
# POKED: flag 00 = 2 and flag 13 = 1 stand in for passing missions 0-1 and
# resting in the barracks (BARRACKS.BLD +0247); docs/reference.md, "Test aids".
TO_TRAINING_CENTER = ("end*2 left*12 end*2 left*2 end*12 left*2 end down*6 end*2 down*6 end*3 "
                      "left*2 end down*2 end*6 down*49 pgdn right*19 pgup*4")
TRAINING_CASES = [dict(case, name="training_" + case["name"]) for case in CASES[:4]] + [
    {
        # Walk to the training center: "Will you enter the Training Center?"
        "name": "training_question",
        "keys": "flag:00=02 flag:13=01 " + TO_TRAINING_CENTER,
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Yes. The original first asks its quiz (a key, then three menus:
        # orig: only, docs/combat.md); then TRAINING.BLD greets and asks
        # about the third mission.
        "name": "training_greeting",
        "keys": "y orig:enter*4",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Yes: the mission's briefing (TRAINING.BLD +05b6).
        "name": "training_briefing",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The 'Mech menu (menu 7).
        "name": "training_mech_menu",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # The Locust: special 1 walks it in, the mission places the target,
        # and the fight asks whether the computer should fight (No first).
        "name": "training_computer_question",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # No: the combat messages menu, on Verbose.
        "name": "training_messages",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Verbose: "See combat graphics", on Yes.
        "name": "training_graphics",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # Yes: the orders menu for Jason's Locust, the view on it with the
        # box, the bar on Begin Fight.
        "name": "training_orders",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
]

# The training center's quiz, shown (chr --show-quiz, docs/combat.md 7).
# The original asks it as built; the rebuild has no check, and its menu
# starts on the right part. So the original's bar is moved onto the right
# part (orig: keys, by DG:28ec's answers for the parts this run draws: 1, 6
# and 9) before each comparison, and Enter takes it on both sides.
QUIZ_CASES = TRAINING_CASES[:5] + [
    {
        # Yes: the message before the quiz, in the big box.
        "name": "quiz_message",
        "keys": "y",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # A key: BTSTATS.CMP, the first part boxed, the part names.
        "name": "quiz_part_1",
        "keys": "enter orig:down*4",
        "match": {"the screen": (0, 0, 320, 200)},
    },
    {
        "name": "quiz_part_2",
        "keys": "enter orig:down*14",
        "match": {"the screen": (0, 0, 320, 200)},
    },
    {
        "name": "quiz_part_3",
        "keys": "enter orig:up*2",
        "match": {"the screen": (0, 0, 320, 200)},
    },
    {
        # The last answer: the training center greets, as without the scene.
        "name": "quiz_greeting",
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
]

# Each sequence is one run of the original: (name, quick cases, --full
# cases, chr's own options).
SEQUENCES = [("town", CASES, FULL_CASES, []), ("training", TRAINING_CASES, [], []),
             ("quiz", QUIZ_CASES, [], ["--show-quiz"])]

# The feeder (tools/capture/snap.asm, docs/reference.md) hands over a key at
# every 4th poll of the keyboard. The main loop polls once a pass, and after
# a key it flushes the keyboard with one more poll, so the town starts with
# 3 passes before the first item, and a key is followed by 2. idle:N is N.
FIRST_PASSES = 3
PASSES_AFTER_KEY = 2


# A test aid in a case's keys: "flag:NN=VV" (hex) sets a flag in both
# programs at that point: a test poke in the original (run.py, data only)
# and set:NN=VV in chr. A case that uses one says so and what play it stands
# in for (docs/reference.md).
FLAGS_AT = 0xD30C  # F:d30c, the flags


def flag_poke(token):
    """The original's poke for a flag:NN=VV token."""
    flag, _, value = token[len("flag:"):].partition("=")
    return "@poke:F:%04x=%02x" % (FLAGS_AT + int(flag, 16), int(value, 16))


def original_token(k):
    """A case's key as the original's script has it: flag: is a poke, and
    orig: marks input that only the original asks for (the training quiz,
    which the rebuild does not show: docs/combat.md)."""
    if k.startswith("flag:"):
        return flag_poke(k)
    if k.startswith("orig:"):
        return k[len("orig:"):]
    return k


def expand(keys):
    out = []
    for token in keys.replace(",", " ").split():
        name, _, count = token.partition("*")
        out += [name] * (int(count) if count else 1)
    return out


def chr_script(cases, upto):
    """The chr script for the case at index `upto`: every item so far."""
    items = []
    in_town = False
    for case in cases[:upto + 1]:
        for k in expand(case["keys"]):
            if k.startswith("orig:"):
                continue  # input only the original asks for (docs/reference.md)
            if k.startswith("flag:"):
                items.append("set:" + k[len("flag:"):])
                continue
            items.append(k)
            if case.get("starts_town"):
                # The answer to the first-time question: the town begins.
                items.append("idle:%d" % FIRST_PASSES)
                in_town = True
            elif in_town and not k.startswith("idle:"):
                items.append("idle:%d" % PASSES_AFTER_KEY)
    return " ".join(items) or "idle:1"


def original_script(cases):
    tokens = [START]
    for case in cases:
        if case.get("keys"):
            tokens.append(" ".join(original_token(k) for k in case["keys"].replace(",", " ").split()))
        tokens.append("@snap:" + case["name"])
    return " ".join(tokens)


def run_sequence(a, sequence, cases, chr_options=()):
    """One run of the original through `cases`, then chr for each case.
    True if every required rectangle and the state match."""
    ref_dir = os.path.join(a.work, "original-" + sequence)
    finished, _ = run.run(a.game_dir, original_script(cases), ref_dir, timeout=900 if a.full else 240)
    if not finished:
        print(f"the original did not reach the end of the {sequence} script")
        return False

    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    ok = True
    for i, case in enumerate(cases):
        name = case["name"]
        shot = os.path.join(a.work, name + ".bmp")
        out = subprocess.run([a.chr, "--game-dir", a.game_dir, "--data-dir", a.data_dir,
                              "--script", chr_script(cases, i), "--screenshot", shot, "--state", *chr_options],
                             check=True, env=env, stdout=subprocess.PIPE, text=True).stdout
        ours_state = [line for line in out.splitlines() if line.startswith("state ")]
        ref_state = state.original_state(os.path.join(ref_dir, name + ".bin"))
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
        if not case.get("state", True):
            ref_state = ours_state = []
        differ = [f"original: {r}\n         chr: {c}" for r, c in zip(ref_state, ours_state) if r != c]
        if len(ref_state) != len(ours_state):
            differ.append("different number of state lines")
        print(f"  {'ok  ' if not differ else 'FAIL'} the game state: {len(differ)} lines differ")
        for d in differ:
            print("    " + d)
        ok = ok and not differ
        for known in case.get("known", []):
            print(f"  known: {known}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--chr", required=True)
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--work", required=True, help="where the captures, screenshots and diffs go")
    ap.add_argument("--full", action="store_true", help="the long cases too")
    a = ap.parse_args()

    missing = [what for what, ok in (("DOSBox-X", os.path.exists(run.DOSBOX)),
                                     ("nasm", shutil.which("nasm")),
                                     ("the game folder", os.path.exists(os.path.join(a.game_dir, "BTECH.EXE"))))
               if not ok]
    if missing:
        print("skipped: missing " + ", ".join(missing))
        return SKIP

    os.makedirs(a.work, exist_ok=True)
    ok = True
    for sequence, quick, full, chr_options in SEQUENCES:
        cases = quick + (full if a.full else [])
        ok = run_sequence(a, sequence, cases, chr_options) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
