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
townspeople's records must be identical, and the cell window chr builds
where the original last built its own (window.py) must hold the same cells.

The quick cases run in ctest (`reference_compare`); --full adds the long
ones (thousands of passes of idling, a walk across MAP1), a few minutes.

Exit status: 0 all required rectangles match, 1 a mismatch, 77 skipped
(no DOSBox-X, nasm or game folder: the check needs all three).
"""
import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare  # noqa: E402
import run  # noqa: E402
import state  # noqa: E402
import window  # noqa: E402

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

# Training mission 2 fought by the computer, with no combat messages and no
# combat graphics: after the three answers it runs on random draws alone.
# With the computer fighting, the original asks once a round whether a key
# is waiting (203b:1015). "arm:1" before the last answer has the feeder
# answer the first such question, so the first capture is after round 1;
# each idle:1 after it is one more round. chr asks the same question and
# stops the fight at the first one its script has no item for; "arm:N"
# gives it N - 1 passes after the key. A capture after the fight has ended
# is taken at "You should return to base now.", where the original waits
# for a key (idle:99 lets the rest of the rounds go by first). The training
# cases before it are only captured: the training sequence compares them.
MISSION_ROUNDS = [
    {
        # The state after round n (the screen is not drawn while they run).
        "name": "mission_round_%d" % n,
        "keys": "arm:1 n" if n == 1 else "idle:1",
        "match": {},
        "fight": True,
    }
    for n in range(1, 9)
]
MISSION_CASES = [dict(case, compare=False) for case in TRAINING_CASES[:9]] + [
    {
        # Yes: the combat messages menu, on Verbose.
        "name": "mission_messages",
        "keys": "y",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
    {
        # None: "See combat graphics", on Yes.
        "name": "mission_graphics",
        "keys": "up*2 enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
    },
] + MISSION_ROUNDS[:1]
MISSION_FULL_CASES = MISSION_ROUNDS[1:] + [
    {
        # The fight is over: "You should return to base now."
        "name": "mission_end",
        "keys": "idle:99",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
        "fight": True,
    },
]

# The same fight with combat messages on (graphics still off): every shot's
# line in window 4, so each capture compares the last shot's text too.
def messages_cases(level, keys):
    """The mission with combat messages `level` ("verbose", "brief"):
    `keys` picks it in the messages menu. (quick cases, long cases)"""
    question = dict(MISSION_CASES[11 - 2], name=level + "_messages")
    graphics = {"name": level + "_graphics", "keys": keys,
                "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}}
    rounds = [dict(case, name=case["name"].replace("mission", level),
                   match={"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}) for case in MISSION_ROUNDS]
    end = dict(MISSION_FULL_CASES[-1], name=level + "_end")
    prefix = MISSION_CASES[:9]
    return prefix + [question, graphics] + rounds[:1], rounds[1:] + [end]


VERBOSE_CASES, VERBOSE_FULL_CASES = messages_cases("verbose", "enter")

# The same fight with no messages and combat graphics on: the shots' poses,
# missiles and explosions redraw the view, and every redraw writes the
# figures' terrain and screen places (docs/combat.md 3.14, quirk 36). The
# state is compared, not the screen.
DRAWN_ROUNDS = [dict(case, name=case["name"].replace("mission", "drawn"),
                     keys="arm:1 y" if n == 1 else case["keys"],
                     match={"the panel": WHOLE_PANEL, "the map": WHOLE_MAP})
                for n, case in enumerate(MISSION_ROUNDS, 1)]
DRAWN_CASES = [dict(case, compare=False) for case in MISSION_CASES[:11]] + DRAWN_ROUNDS[:1]
DRAWN_FULL_CASES = DRAWN_ROUNDS[1:] + [dict(MISSION_FULL_CASES[-1], name="drawn_end")]
BRIEF_CASES, BRIEF_FULL_CASES = messages_cases("brief", "up enter")

# The same mission fought by the player, Verbose messages and graphics on:
# Begin Fight with no orders, round after round, so the party stands and
# the enemy comes on (docs/combat.md 2.6). Each capture is the next round's
# orders menu. Then Flee: escaped, "You should return to base now.";
# not, "You do not escape!" and "Press a key.", both waiting for a key.
ORDERS_ROUNDS = [
    {
        "name": "orders_round_%d" % n,
        "keys": "enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
        "fight": True,
    }
    for n in range(1, 9)
]
ORDERS_CASES = [dict(case, compare=False) for case in TRAINING_CASES[:11]] + [
    dict(TRAINING_CASES[11], name="orders_menu", fight=True)] + ORDERS_ROUNDS[:1]
ORDERS_FULL_CASES = ORDERS_ROUNDS[1:] + [
    {
        "name": "orders_flee",
        "keys": "up enter",
        "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP},
        "fight": True,
    },
]

# The player's move orders (203b:1c1f, docs/combat.md 2.6): Walk with the
# map cursor, Run over the walk orders (asked, then deleted), Jump with a
# Locust (no jump jets), then rounds fought on the run orders.
SCREEN = {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}
MOVE_CASES = [dict(case, compare=False) for case in ORDERS_CASES[:12]] + [
    # Walk: "You have N movement points left", the cursor at the unit.
    {"name": "move_walk", "keys": "up*9 enter", "match": SCREEN, "fight": True},
]
MOVE_FULL_CASES = [
    # A step east: a tentative waypoint and its path's arrows.
    {"name": "move_step", "keys": "right", "match": SCREEN, "fight": True},
    # Further, diagonally and past the box.
    {"name": "move_steps", "keys": "pgdn right*3 down", "match": SCREEN, "fight": True},
    # Back onto the first step's cell, and Enter: the menu, its bar on Walk.
    {"name": "move_accept", "keys": "up left*3 enter", "match": SCREEN, "fight": True},
    # Walk again: a second waypoint after the first.
    {"name": "move_walk_again", "keys": "enter left*2 up*3", "match": SCREEN, "fight": True},
    {"name": "move_accept_again", "keys": "enter", "match": SCREEN, "fight": True},
    # Run: "You are now Walking. Do you wish to Run instead?"
    {"name": "move_run_question", "keys": "down enter", "match": SCREEN, "fight": True},
    # Yes: the orders deleted, a key.
    {"name": "move_run_deleted", "keys": "y", "match": SCREEN, "fight": True},
    {"name": "move_run_cursor", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "move_run_steps", "keys": "right*4 down*2 enter", "match": SCREEN, "fight": True},
    # Jump with no jump jets: the reason and "Press a key."
    {"name": "move_jump", "keys": "down enter", "match": SCREEN, "fight": True},
    # A key, then Begin Fight: the rounds on the run orders.
    {"name": "move_round_1", "keys": "enter down*7 enter", "match": SCREEN, "fight": True},
    {"name": "move_round_2", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "move_round_3", "keys": "enter", "match": SCREEN, "fight": True},
]

# Thirteen Walk orders (bug 17), each on the unit's own cell (the cursor
# out and back): they cost nothing, and in this area (0xcc) bug 16 never
# finds a repeat. The thirteenth goes into figure 1's first order, in the
# original and, with fix_order_bound off (the default), in chr.
WAYPOINT_CASES = [dict(case, compare=False) for case in ORDERS_CASES[:12]] + [
    {"name": "waypoints_12", "keys": "up*9 enter right left enter " + " ".join(
        ["enter right left enter"] * 11), "match": SCREEN, "fight": True},
]
WAYPOINT_FULL_CASES = [
    {"name": "waypoints_13", "keys": "enter right left enter", "match": SCREEN, "fight": True},
    # Begin Fight: figure 1 is not active, so its order is never read.
    {"name": "waypoints_round", "keys": "down*9 enter", "match": SCREEN, "fight": True},
]

# Use Weapons (1d43:0004, docs/combat.md 2.6): the list, the first weapon
# at the enemy Locust through the target picker, Done, then rounds fought
# with it, and a Flee after the shots have drawn numbers.
WEAPON_CASES = [dict(case, compare=False) for case in ORDERS_CASES[:12]] + [
    # The list in window 5, the bar on the first weapon.
    {"name": "weapons_list", "keys": "up*6 enter", "match": SCREEN, "fight": True},
]
WEAPON_FULL_CASES = [
    # The first weapon: "Choose a target", the picker on the enemy Locust.
    # The view's last column is past the map file's east edge: terrain.
    {"name": "weapons_picker", "keys": "enter", "match": SCREEN, "fight": True},
    # Fire: back to the list, the weapon's target and range band shown.
    {"name": "weapons_targeted", "keys": "enter", "match": SCREEN, "fight": True},
    # Done (up from the first item wraps to it): the orders menu.
    {"name": "weapons_done", "keys": "up enter", "match": SCREEN, "fight": True},
] + [
    {"name": "weapons_round_%d" % n, "keys": "up enter" if n == 1 else "enter", "match": SCREEN, "fight": True}
    for n in range(1, 5)
]

# Kick (203b:2231): the picker for target slot 11, then Begin Fight and
# rounds with the kick ordered.
KICK_CASES = [dict(case, compare=False) for case in ORDERS_CASES[:12]] + [
    # "Choose the 'Mech to kick", the picker on the enemy Locust.
    # Its last map column is past the map file's edge, as in weapons_picker.
    {"name": "kick_picker", "keys": "up*5 enter", "match": SCREEN, "fight": True},
]
KICK_FULL_CASES = [
    # Target here: back to the orders menu, its bar on Kick.
    {"name": "kick_targeted", "keys": "enter", "match": SCREEN, "fight": True},
] + [
    {"name": "kick_round_%d" % n, "keys": "down*5 enter" if n == 1 else "enter", "match": SCREEN, "fight": True}
    for n in range(1, 4)
] + [
    # The left hip actuator lost (a test poke of byte 0x24 standing in for
    # a critical hit): Kick says it cannot, and a key.
    {"name": "kick_no_hip", "keys": "mech:0:24=c7 up*5 enter", "match": SCREEN, "fight": True},
    # The key: the orders menu, its bar on Kick.
    {"name": "kick_no_hip_menu", "keys": "enter", "match": SCREEN, "fight": True},
    # Begin Fight: the round drops the kick still in slot 11.
    {"name": "kick_no_hip_round", "keys": "down*5 enter", "match": SCREEN, "fight": True},
]

# Scan Unit (203b:2591) and the scan list (15ab:1467): Friends (the unit
# itself, Next comes back to it), Done, Enemies (the enemy Locust and its
# direction), Done, Cancel, then a round.
SCAN_CASES = [dict(case, compare=False) for case in ORDERS_CASES[:12]] + [
    # Friends / Enemies / Cancel in window 3.
    {"name": "scan_menu", "keys": "up*3 enter", "match": SCREEN, "fight": True},
    # Friends: the unit's heading and screen; Next, Detail, Done.
    {"name": "scan_friends", "keys": "enter", "match": SCREEN, "fight": True},
]
SCAN_FULL_CASES = [
    # Next: past the inactive figures and back to the unit.
    {"name": "scan_friends_next", "keys": "enter", "match": SCREEN, "fight": True},
    # Done: the orders menu, its bar on Scan Unit.
    {"name": "scan_friends_done", "keys": "down*2 enter", "match": SCREEN, "fight": True},
    # Enemies: the enemy Locust, its direction, the view on it. The view's
    # last column is past the map file's edge, as in weapons_picker.
    {"name": "scan_enemies", "keys": "enter down enter", "match": SCREEN, "fight": True},
    {"name": "scan_enemies_done", "keys": "down*2 enter", "match": SCREEN, "fight": True},
    # Cancel.
    {"name": "scan_cancel", "keys": "enter down*2 enter", "match": SCREEN, "fight": True},
    # Begin Fight from Scan Unit.
    {"name": "scan_round", "keys": "down*3 enter", "match": SCREEN, "fight": True},
]

# Mission 3 (training mode 3, docs/combat.md 1.3): three to six enemy
# people against Jason's Locust (17dc:0d49's people), then rounds fought.
PEOPLE_CASES = [dict(case, name=case["name"].replace("training_", "people_")) for case in TRAINING_CASES]
PEOPLE_CASES[4] = dict(PEOPLE_CASES[4], keys=PEOPLE_CASES[4]["keys"].replace("flag:00=02", "flag:00=03"))
PEOPLE_CASES = [dict(case, compare=False) for case in PEOPLE_CASES[:5]] + PEOPLE_CASES[5:]
# From round 6 the soldiers' lines of sight read memory before the cell
# window, the area grids (docs/combat.md 1.2.1). The fight goes on: they
# keep to the area's south edge, and none falls in 40 rounds (all 40
# match); 20 are kept here.
PEOPLE_FULL_CASES = [
    {"name": "people_round_%d" % n, "keys": "enter", "match": SCREEN, "fight": True}
    for n in range(1, 21)
]

# The computer hands the fight back (a key at the round's poll) and the
# player flees, and fails: 52 idle passes in town before the walk to the
# training center and six computer-fought rounds give a flee draw with
# r & 3 = 0 (docs/combat.md 2.7). The unit stays under the computer, so
# the orders screen shows its plan (1e31:03ab with show).
HANDOVER_CASES = [dict(case, compare=False) for case in MISSION_CASES[:4]] + [
    dict(MISSION_CASES[4], keys="idle:52 " + MISSION_CASES[4]["keys"], compare=False)] + [
    dict(case, compare=False) for case in MISSION_CASES[5:]] + [
    dict(case, name=case["name"].replace("mission", "handover")) for case in MISSION_ROUNDS[1:6]] + [
    # A key at the poll: the orders menu, "The computer is moving this unit".
    {"name": "handover_menu", "keys": "enter", "match": SCREEN, "fight": True},
    # Flee, and fail: "You do not escape!", "Press a key."
    {"name": "handover_flee_failed", "keys": "up enter", "match": SCREEN, "fight": True},
    # The key: the round goes on; the next orders menu.
    {"name": "handover_next_round", "keys": "enter", "match": SCREEN, "fight": True},
]

# Each sequence is one run of the original: (name, quick cases, --full
# cases, chr's own options).
SEQUENCES = [("town", CASES, FULL_CASES, []), ("training", TRAINING_CASES, [], []),
             ("quiz", QUIZ_CASES, [], ["--show-quiz"]), ("mission", MISSION_CASES, MISSION_FULL_CASES, []),
             ("verbose", VERBOSE_CASES, VERBOSE_FULL_CASES, []), ("drawn", DRAWN_CASES, DRAWN_FULL_CASES, []),
             ("brief", [], BRIEF_CASES + BRIEF_FULL_CASES, []),
             ("orders", ORDERS_CASES, ORDERS_FULL_CASES, []),
             ("moves", MOVE_CASES, MOVE_FULL_CASES, []),
             ("waypoints", [], WAYPOINT_CASES + WAYPOINT_FULL_CASES, []),
             ("weapons", WEAPON_CASES, WEAPON_FULL_CASES, []),
             ("kick", KICK_CASES, KICK_FULL_CASES, []),
             ("scan", SCAN_CASES, SCAN_FULL_CASES, []),
             ("people", PEOPLE_CASES, PEOPLE_FULL_CASES, []),
             ("handover", [], HANDOVER_CASES, [])]

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


RECORDS_AT = 0xC724  # F:c724, the eight 'Mech records (docs/formats/mech.md)


def mech_poke(token):
    """The original's poke for a mech:R:BB=VV token: one byte inside the
    eight 'Mech records, nothing else."""
    try:
        record, _, rest = token[len("mech:"):].partition(":")
        byte, _, value = rest.partition("=")
        r, b, v = int(record, 16), int(byte, 16), int(value, 16)
    except ValueError:
        raise ValueError(f"bad record poke {token!r}: use mech:R:BB=VV (hex)")
    if not (0 <= r < 8 and 0 <= b < 0x7d and 0 <= v <= 0xff):
        raise ValueError(f"refused record poke {token!r}: record 0-7, byte below 7d, value one byte")
    return "@poke:F:%04x=%02x" % (RECORDS_AT + 0x7d * r + b, v)


def original_token(k):
    """A case's key as the original's script has it: flag: is a poke, and
    orig: marks input that only the original asks for (the training quiz,
    which the rebuild does not show: docs/combat.md)."""
    if k.startswith("flag:"):
        return flag_poke(k)
    if k.startswith("mech:"):
        return mech_poke(k)
    if k.startswith("orig:"):
        return k[len("orig:"):]
    if k.startswith("arm:"):
        return "@" + k
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
    passes_after = None
    for case in cases[:upto + 1]:
        for k in expand(case["keys"]):
            if k.startswith("orig:"):
                continue  # input only the original asks for (docs/reference.md)
            if k.startswith("arm:"):
                passes_after = int(k[len("arm:"):]) - 1  # for the next key only
                continue
            if k.startswith("flag:"):
                items.append("set:" + k[len("flag:"):])
                continue
            if k.startswith("mech:"):
                items.append(k)  # applied as the next key is read; no passes of its own
                continue
            items.append(k)
            if case.get("starts_town"):
                # The answer to the first-time question: the town begins.
                items.append("idle:%d" % FIRST_PASSES)
                in_town = True
            elif in_town and not k.startswith("idle:"):
                n = PASSES_AFTER_KEY if passes_after is None else passes_after
                passes_after = None
                if n > 0:
                    items.append("idle:%d" % n)
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
    (True if every required rectangle and the state match, the report.)"""
    report = []
    say = report.append
    ref_dir = os.path.join(a.work, "original-" + sequence)
    finished, _ = run.run(a.game_dir, original_script(cases), ref_dir, timeout=900 if a.full else 240)
    if not finished:
        say(f"the original did not reach the end of the {sequence} script")
        return False, report

    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    ok = True
    for i, case in enumerate(cases):
        name = case["name"]
        if not case.get("compare", True):
            continue  # compared by another sequence
        shot = os.path.join(a.work, name + ".bmp")
        # The cell window too, built where the original last built its own.
        at, ref_window = window.original_window(os.path.join(ref_dir, name + ".bin"))
        out = subprocess.run([a.chr, "--game-dir", a.game_dir, "--data-dir", a.data_dir,
                              "--script", chr_script(cases, i), "--screenshot", shot,
                              "--fight-state" if case.get("fight") else "--state",
                              "--window", "%x,%x" % at, *chr_options],
                             check=True, env=env, stdout=subprocess.PIPE, text=True).stdout
        ours_state = [line for line in out.splitlines() if line.startswith(("state ", "window "))]
        ref_state = state.original_state(os.path.join(ref_dir, name + ".bin"), fight=case.get("fight", False))
        ref_state += ref_window
        ref = compare.reference_pixels(os.path.join(ref_dir, name + ".bin"))
        ours = compare.bmp_pixels(shot)
        compare.write_diff(os.path.join(a.work, name + "-diff.png"), ref, ours)
        say(f"== {name}")
        for region in ("panel", "play"):
            n = len(compare.compare(ref, ours, region))
            say(f"  {region}: {n} pixels differ")
        for label, rect in case["match"].items():
            n = len(compare.compare_rect(ref, ours, rect))
            say(f"  {'ok  ' if n == 0 else 'FAIL'} {label}: {n} pixels differ")
            ok = ok and n == 0
        if not case.get("state", True):
            ref_state = ours_state = []
        differ = [f"original: {r}\n         chr: {c}" for r, c in zip(ref_state, ours_state) if r != c]
        if len(ref_state) != len(ours_state):
            differ.append("different number of state lines")
        say(f"  {'ok  ' if not differ else 'FAIL'} the game state: {len(differ)} lines differ")
        for d in differ:
            say("    " + d)
        ok = ok and not differ
        for known in case.get("known", []):
            say(f"  known: {known}")
    return ok, report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--chr", required=True)
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--work", required=True, help="where the captures, screenshots and diffs go")
    ap.add_argument("--full", action="store_true", help="the long cases too")
    ap.add_argument("--sequence", action="append", help="run only this sequence (may be repeated)")
    a = ap.parse_args()

    missing = [what for what, ok in (("DOSBox-X", os.path.exists(run.DOSBOX)),
                                     ("nasm", shutil.which("nasm")),
                                     ("the game folder", os.path.exists(os.path.join(a.game_dir, "BTECH.EXE"))))
               if not ok]
    if missing:
        print("skipped: missing " + ", ".join(missing))
        return SKIP

    os.makedirs(a.work, exist_ok=True)
    # The sequences are separate runs of the original, each in its own
    # copy of the game folder (run.py), so they run side by side.
    jobs = [(sequence, quick + (full if a.full else []), chr_options)
            for sequence, quick, full, chr_options in SEQUENCES
            if not a.sequence or sequence in a.sequence]
    ok = True
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(jobs) or 1) as pool:
        results = [pool.submit(run_sequence, a, sequence, cases, chr_options)
                   for sequence, cases, chr_options in jobs]
        for result in results:
            passed, report = result.result()
            print("\n".join(report), flush=True)
            ok = ok and passed
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
