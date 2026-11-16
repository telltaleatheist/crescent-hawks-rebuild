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
    # A key: the party is put where Jason's 'Mech stands (17dc:134b) and
    # the mission's walk begins; back to the door it ends, and the script
    # goes on; a key, and the town. The way back was found with chr's step
    # check.
    {"name": "mission_walk", "keys": "enter home*8", "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}},
    {"name": "mission_over", "keys": "home*8 left*30", "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}},
    {"name": "mission_in_town", "keys": "enter", "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}},
    {"name": "mission_walk_on", "keys": "down", "match": {"the panel": WHOLE_PANEL, "the map": WHOLE_MAP}},
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

# Mission 0 (training mode 0, docs/combat.md 1.3): the walking course.
# Flag 00 stays 0, as in a new game.
COURSE_CASES = [dict(case, name=case["name"].replace("training_", "course_")) for case in TRAINING_CASES[:9]]
# No pokes: the course run starts from the new game as it is.
COURSE_CASES[4] = dict(COURSE_CASES[4], keys=COURSE_CASES[4]["keys"].replace("flag:00=02 flag:13=01 ", ""))
COURSE_CASES = [dict(case, compare=False) for case in COURSE_CASES[:8]] + [
    # The Locust walks in, and the course starts: the party waits in its 'Mech.
    dict(COURSE_CASES[8], name="course_start", match=SCREEN)]
# The course's way to the southeast corner and back (found with chr's own
# step check, which the original agrees with along it).
COURSE_TO_CORNER = "pgdn*14 right*4 pgdn*2 down*8 pgdn*21 right*12"
COURSE_BACK_1 = "home*24 up*6 right down pgup down pgup down pgup down pgup*2 up*3"
COURSE_BACK_2 = "up*2 home*12 left*30"
COURSE_FULL_CASES = [
    {"name": "course_step_%d" % n, "keys": "right", "match": SCREEN} for n in range(1, 4)
] + [
    # The rest of the way to the corner: "You've made it to the southeast
    # corner", a key.
    {"name": "course_corner", "keys": COURSE_TO_CORNER, "match": SCREEN},
    # The key, and half the way back.
    {"name": "course_returning", "keys": "enter " + COURSE_BACK_1, "match": SCREEN},
    # The rest: at the door the mission ends and the script goes on.
    {"name": "course_ended", "keys": COURSE_BACK_2, "match": SCREEN},
    # A key: the script ends (special 3 hands the 'Mech back) and the party
    # is in town again, on foot.
    {"name": "course_in_town", "keys": "enter", "match": SCREEN},
    {"name": "course_walk_on", "keys": "down*3", "match": SCREEN},
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

# Training mission 4 (flag 00 poked to 4): one enemy 'Mech, and no
# computer to fight for the party (F:0090), so no question about it. The
# Locust flees at the first orders menu; the walk back from Jason's 'Mech
# to the door, the verdict on running away, and the town.
FOURTH_CASES = [dict(case, compare=False) for case in TRAINING_CASES[:4]] + [
    dict(TRAINING_CASES[4], name="fourth_question",
         keys=TRAINING_CASES[4]["keys"].replace("flag:00=02", "flag:00=04"), match=SCREEN),
    dict(TRAINING_CASES[5], name="fourth_greeting", match=SCREEN),
    dict(TRAINING_CASES[6], name="fourth_briefing", match=SCREEN),
    dict(TRAINING_CASES[7], name="fourth_mech_menu", match=SCREEN),
    {"name": "fourth_messages", "keys": "enter", "match": SCREEN},
    {"name": "fourth_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "fourth_orders", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "fourth_fled", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "fourth_walk", "keys": "enter", "match": SCREEN},
    {"name": "fourth_over", "keys": "left*4", "match": SCREEN},
    {"name": "fourth_in_town", "keys": "enter", "match": SCREEN},
]

# Training mission 7 (flag 00 poked to 7): always the raid. Its message,
# the options without the computer question, then Begin Fight round after
# round: a story line after each (the second with the place's picture,
# after the third map 11 in the town's place), until the Jenners bring the
# Locust down; the rest of the lines, the results and the script.
def raid_round(n, keys):
    return {"name": "raid_%02d" % n, "keys": keys, "match": SCREEN, "fight": True}
RAID_CASES = [dict(case, compare=False) for case in FOURTH_CASES[:4]] + [
    dict(FOURTH_CASES[4], name="raid_question",
         keys=FOURTH_CASES[4]["keys"].replace("flag:00=04", "flag:00=07")),
    dict(FOURTH_CASES[5], name="raid_greeting"),
    dict(FOURTH_CASES[6], name="raid_briefing"),
    dict(FOURTH_CASES[7], name="raid_message"),
    {"name": "raid_mechs", "keys": "enter", "match": SCREEN},
    {"name": "raid_computer", "keys": "enter", "match": SCREEN},
    {"name": "raid_messages", "keys": "enter", "match": SCREEN},
    {"name": "raid_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "raid_orders", "keys": "enter", "match": SCREEN, "fight": True},
    raid_round(1, "enter"), raid_round(3, "enter*2"), raid_round(5, "enter*2"), raid_round(6, "enter"),
    raid_round(9, "enter*3"), raid_round(13, "enter*4"), raid_round(17, "enter*4"),
    {"name": "raid_18", "keys": "enter", "match": SCREEN},
    {"name": "raid_20", "keys": "enter*2", "match": SCREEN},
    {"name": "raid_21", "keys": "enter", "match": SCREEN},
    {"name": "raid_22", "keys": "enter", "match": SCREEN},
]

# The third mission's first try (flag 13 not poked): the commandant stops
# Jason and takes the 'Mech back (TRAINING.BLD +04f3, special 3). He sleeps
# in the barracks (BARRACKS.BLD +0243: flag 13 + 1, the story, flag 14 = 0)
# and goes back for the briefing, the quiz asked again.
REST_CASES = [dict(case, compare=False) for case in TRAINING_CASES[:4]] + [
    dict(TRAINING_CASES[4], name="rest_question",
         keys=TRAINING_CASES[4]["keys"].replace("flag:13=01", "flag:01=01")),
    dict(TRAINING_CASES[5], name="rest_greeting"),
    {"name": "rest_commandant", "keys": "y", "match": SCREEN},
    {"name": "rest_handed_back", "keys": "enter", "match": SCREEN},
    # On foot to the Barracks door (MAP1 entrance 6).
    {"name": "rest_barracks_question", "keys": "up home*2 down*2 left*2 down right*2 pgup down*2 left*23 home up*85 left home up left home up*15 home", "match": SCREEN},
    {"name": "rest_barracks", "keys": "y", "match": SCREEN},
    {"name": "rest_sleep", "keys": "down enter", "match": SCREEN},
    {"name": "rest_story_2", "keys": "enter", "match": SCREEN},
    {"name": "rest_story_4", "keys": "enter*2", "match": SCREEN},
    {"name": "rest_story_7", "keys": "enter*3", "match": SCREEN},
    {"name": "rest_menu_again", "keys": "enter", "match": SCREEN},
    {"name": "rest_leave", "keys": "up enter", "match": SCREEN},
    # The same way back (each step turned round) to the Training Center.
    {"name": "rest_back_question", "keys": "pgdn down*15 pgdn right down pgdn right down*85 pgdn right*23 up*2 end left*2 up right*2 up*2 pgdn*2 down home up*2", "match": SCREEN},
    # Flag 14 is 0 again: the quiz first (orig: only), then the greeting.
    {"name": "rest_back_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    # Flag 13 is set now: the third mission's briefing (TRAINING.BLD +05b6).
    {"name": "rest_briefing", "keys": "y", "match": SCREEN},
]

# Each sequence is one run of the original: (name, quick cases, --full
# cases, chr's own options).
# Mission 1, the marked spot, reached by keys alone after the course: flag
# 14 (set by TRAINING.BLD as a mission starts) runs down over 255 town
# ticks, an idle tick every 11 polls; then the training center again, the
# quiz on the original's side (the copy we read always passes it), the
# second mission in the Wasp, which has hands. The spot is drawn at random
# (spot 3 here); the way to it and back was found with chr's step check.
SPOT_CASES = [dict(case, compare=False) for case in COURSE_CASES + COURSE_FULL_CASES] + [
    {"name": "spot_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "spot_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "spot_briefing", "keys": "y", "match": SCREEN},
    {"name": "spot_start", "keys": "enter down enter", "match": SCREEN},
    {"name": "spot_reached", "keys": "pgup*10 right*21 pgdn right*3 pgup right*18", "match": SCREEN},
    {"name": "spot_back", "keys": "enter end*2 down*4 end left*55", "match": SCREEN},
    {"name": "spot_in_town", "keys": "enter", "match": SCREEN},
    {"name": "spot_walk_on", "keys": "down", "match": SCREEN},
]

# The long run with no pokes, from the new game on: the course and the
# spot (missions 0 and 1, as `course` and `spot`), then the third mission's
# first try: flag 14 runs down, the commandant sends Jason to rest, the
# Barracks, and back for the third mission.
CHAIN_CASES = [dict(case, compare=False) for case in SPOT_CASES] + [
    {"name": "chain_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_commandant", "keys": "y", "match": SCREEN},
    {"name": "chain_handed_back", "keys": "enter", "match": SCREEN},
    # One step east, then the `rest` sequence's ways there and back.
    {"name": "chain_barracks_question", "keys": "right up home*2 down*2 left*2 down right*2 pgup down*2 left*23 home up*85 left home up left home up*15 home", "match": SCREEN},
    {"name": "chain_barracks", "keys": "y", "match": SCREEN},
    {"name": "chain_sleep", "keys": "down enter", "match": SCREEN},
    {"name": "chain_story_4", "keys": "enter*3", "match": SCREEN},
    {"name": "chain_story_7", "keys": "enter*3", "match": SCREEN},
    {"name": "chain_menu_again", "keys": "enter", "match": SCREEN},
    {"name": "chain_leave", "keys": "up enter", "match": SCREEN},
    {"name": "chain_back_question", "keys": "pgdn down*15 pgdn right down pgdn right down*85 pgdn right*23 up*2 end left*2 up right*2 up*2 pgdn*2 down home up*2", "match": SCREEN},
    {"name": "chain_back_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_briefing", "keys": "y", "match": SCREEN},
    # The third mission in the Wasp (the menu's bar is where mission 1 left
    # it), the computer fighting, graphics off: as in `mission`.
    {"name": "chain_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_computer_question", "keys": "enter", "match": SCREEN},
    {"name": "chain_messages", "keys": "y", "match": SCREEN},
    {"name": "chain_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "chain_round_1", "keys": "arm:1 n", "match": SCREEN, "fight": True},
] + [{"name": "chain_round_%d" % n, "keys": "idle:1", "match": SCREEN, "fight": True} for n in range(2, 17)] + [
    {"name": "chain_end", "keys": "idle:99", "match": SCREEN, "fight": True},
    # The walk back to the door (found with chr), the verdict and the town.
    {"name": "chain_walk", "keys": "enter home*8", "match": SCREEN},
    {"name": "chain_over", "keys": "home*7 left*30", "match": SCREEN},
    {"name": "chain_in_town", "keys": "enter", "match": SCREEN},
    {"name": "chain_walk_on", "keys": "down", "match": SCREEN},
    # Flag 14 runs down again; mission 3 (enemy people) in the Wasp, the
    # computer fighting: the last falls in round 13.
    {"name": "chain_people_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_people_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_people_briefing", "keys": "y", "match": SCREEN},
    {"name": "chain_people_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_people_computer", "keys": "enter", "match": SCREEN},
    {"name": "chain_people_messages", "keys": "y", "match": SCREEN},
    {"name": "chain_people_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "chain_people_round_1", "keys": "arm:1 n", "match": SCREEN, "fight": True},
] + [{"name": "chain_people_round_%d" % n, "keys": "idle:1", "match": SCREEN, "fight": True} for n in range(2, 14)] + [
    {"name": "chain_people_end", "keys": "idle:99", "match": SCREEN, "fight": True},
    {"name": "chain_people_walk", "keys": "enter home*13 left home*2 up*4 right down pgup down pgup down pgup down pgup*2 up*5 home*12 left*30", "match": SCREEN},
    {"name": "chain_people_in_town", "keys": "enter", "match": SCREEN},
    {"name": "chain_people_walk_on", "keys": "down", "match": SCREEN},
    # Mission 4: no computer question; the Locust flees at the first orders
    # menu, as in `fourth`.
    {"name": "chain_fourth_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_fourth_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_fourth_briefing", "keys": "y", "match": SCREEN},
    {"name": "chain_fourth_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_fourth_messages", "keys": "enter", "match": SCREEN},
    {"name": "chain_fourth_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "chain_fourth_orders", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_fourth_fled", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_fourth_walk", "keys": "enter home*3", "match": SCREEN},
    {"name": "chain_fourth_over", "keys": "left", "match": SCREEN},
    {"name": "chain_fourth_in_town", "keys": "enter", "match": SCREEN},
    {"name": "chain_fourth_walk_on", "keys": "down", "match": SCREEN},
    # Mission 4 again, won by orders: the Wasp loses a fair fight with the
    # Locust, so the Chameleon, every weapon on the Locust, one waypoint.
    {"name": "chain_win_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_win_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_win_briefing", "keys": "y", "match": SCREEN},
    {"name": "chain_win_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_win_messages", "keys": "down enter", "match": SCREEN},
    {"name": "chain_win_graphics", "keys": "up*2 enter", "match": SCREEN},
    {"name": "chain_win_orders", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_win_weapons", "keys": "up*6 enter enter enter down enter enter down enter enter down enter enter down enter enter down enter enter down enter enter down enter enter down enter enter down enter", "match": SCREEN, "fight": True},
    {"name": "chain_win_walk", "keys": "enter pgdn*4 right*4 enter", "match": SCREEN, "fight": True},
    {"name": "chain_win_round_1", "keys": "down*9 enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_win_round_%d" % n, "keys": "enter", "match": SCREEN, "fight": True} for n in range(2, 7)] + [
    {"name": "chain_win_end", "keys": "idle:99", "match": SCREEN, "fight": True},
    {"name": "chain_win_walk_back", "keys": "enter home*7", "match": SCREEN},
    {"name": "chain_win_over", "keys": "left*5", "match": SCREEN},
    {"name": "chain_win_in_town", "keys": "enter", "match": SCREEN},
    {"name": "chain_win_walk_on", "keys": "down", "match": SCREEN},
    # Mission 5, two 'Mechs, by orders in the Chameleon. Fewer weapons, or
    # it overheats: the two Medium Lasers and the Machine Guns on figure 12,
    # one waypoint. 12 falls in round 8; the first Medium Laser is gone by
    # then, so the other three go on 13 with a waypoint south, where it can
    # be seen; 13 falls in round 15.
    {"name": "chain_five_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_five_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_five_briefing", "keys": "y", "match": SCREEN},
    {"name": "chain_five_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_five_computer", "keys": "enter", "match": SCREEN},
    {"name": "chain_five_messages", "keys": "n", "match": SCREEN},
    {"name": "chain_five_graphics", "keys": "enter", "match": SCREEN},
    {"name": "chain_five_orders", "keys": "n", "match": SCREEN, "fight": True},
    {"name": "chain_five_weapons", "keys": "up*6 enter up*9 enter enter down down down down enter enter down down down enter enter down enter enter down enter", "match": SCREEN, "fight": True},
    {"name": "chain_five_walk", "keys": "enter pgdn*4 right*4 enter", "match": SCREEN, "fight": True},
    {"name": "chain_five_round_1", "keys": "down*9 enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_five_round_%d" % n, "keys": "enter", "match": SCREEN, "fight": True} for n in range(2, 9)] + [
    {"name": "chain_five_round_9", "keys": "up*6 enter up*9 down*4 enter enter down*3 enter enter down enter enter down enter enter down*4 enter down*9 enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_five_round_%d" % n, "keys": "enter", "match": SCREEN, "fight": True} for n in range(10, 16)] + [
    {"name": "chain_five_end", "keys": "idle:99", "match": SCREEN, "fight": True},
    {"name": "chain_five_walk_back", "keys": "enter home*11", "match": SCREEN},
    {"name": "chain_five_over", "keys": "left", "match": SCREEN},
    {"name": "chain_five_in_town", "keys": "enter", "match": SCREEN},
    {"name": "chain_five_walk_on", "keys": "down", "match": SCREEN},
    # Mission 6: its one draw lands on the raid here (r & 1), the keys as in
    # `raid`.
    {"name": "chain_raid_waited", "keys": "idle:2900 up*5", "match": SCREEN},
    {"name": "chain_raid_greeting", "keys": "y orig:enter*4", "match": SCREEN},
    {"name": "chain_raid_briefing", "keys": "y", "match": SCREEN},
    {"name": "chain_raid_mech_menu", "keys": "enter", "match": SCREEN},
    {"name": "chain_raid_message", "keys": "enter", "match": SCREEN},
    {"name": "chain_raid_computer", "keys": "enter", "match": SCREEN},
    {"name": "chain_raid_messages", "keys": "enter", "match": SCREEN},
    {"name": "chain_raid_graphics", "keys": "enter", "match": SCREEN},
    {"name": "chain_raid_options", "keys": "up*2 enter", "match": SCREEN},
    {"name": "chain_raid_orders", "keys": "enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_raid_round_%d" % n, "keys": "enter", "match": SCREEN, "fight": True} for n in range(1, 6)] + [
    # Then Begin Fight (and the story lines' keys) on until the Jenners bring
    # the Chameleon down and the town comes back.
] + [{"name": "chain_raid_key_%d" % n, "keys": "enter*5", "match": SCREEN, "fight": True} for n in range(10, 46, 5)] + [
    {"name": "chain_raid_key_%d" % n, "keys": "enter", "match": SCREEN, "fight": True} for n in range(46, 58)] + [
    # Out of the town to the north: the town is map 11 now (outdoors,
    # flag 04), and past its top edge lies area 0xbc, all terrain. Random
    # encounters come up on the way (flag 24 is 0x3f), none in reach.
    {"name": "chain_out_1", "keys": "up*46 pgup up home up*3 left down home down home down home down home", "match": SCREEN},
    {"name": "chain_out_2", "keys": "home up left home up*5 left home up*5 left down home down home down home down home down home down home down home down home down home down home down home down home down home down home down home down pgdn right*12", "match": SCREEN},
    {"name": "chain_out_3", "keys": "right*2 down*2 home down home down home down home down home down home down home down home down home down home down home down home down home down home down pgdn right*13 pgdn*2 down home*2 down home down home down home down home down home", "match": SCREEN},
    {"name": "chain_out_4", "keys": "down home down home down home down home down home down home down home down home*3 down home down home down home down home down home down home down home*2 up left down pgdn*2 left home down end down home*2 up*11 pgup*5", "match": SCREEN},
    {"name": "chain_out_area", "keys": "up*10", "match": SCREEN},
]

# The chain's other branch from the walk out, its own sequence: a step west
# meets a random encounter in reach, four soldiers. Declining does not get
# away this time (r & 3 is 0), and the fight's questions follow.
CHAIN_DEATH_CASES = [dict(case, compare=False) for case in CHAIN_CASES] + [
    {"name": "chain_enc", "keys": "home", "match": SCREEN},
    {"name": "chain_enc_no", "keys": "n", "match": SCREEN},
    {"name": "chain_enc_computer", "keys": "enter", "match": SCREEN},
    # The computer fights (Yes), no combat messages, no combat graphics,
    # then round after round as in MISSION_ROUNDS. Jason, alone on foot,
    # is killed in round 5: the game is over (DG:014a is 0), a box and a
    # key. The question after the key is not compared: the original's
    # capture after Enter still shows the box.
] + [{"name": "chain_enc_round_%d" % n, "keys": "y enter arm:1 n" if n == 1 else "idle:1", "match": {}, "fight": True}
     for n in range(1, 6)] + [
    {"name": "chain_enc_over", "keys": "idle:1", "match": SCREEN},
]

# Poked: from chain_out_4, Jason is put in a 'Mech (docs/reference.md,
# "Pokes"): the raid's wrecked Chameleon record 0 gets a first letter, armour
# and structure, and character 0 byte 0x0c says he pilots record 0. The
# values are our own test values, not the template's (docs/reference.md).
# Then the walk west to the first
# encounter in reach, five soldiers, which the computer fights.
MECH_POKES = " ".join(["char:0:0c=00", "mech:0:00=43"] + ["mech:0:%02x=20" % b for b in range(0x11, 0x1c)]
                      + ["mech:0:%02x=10" % b for b in range(0x1c, 0x24)] + ["mech:0:24=ff"])
CHAIN_MECH_POKED_CASES = [dict(case, compare=False) for case in CHAIN_CASES[:CHAIN_CASES.index(
    next(c for c in CHAIN_CASES if c["name"] == "chain_out_4")) + 1]] + [
    # The state only: the original's screen still shows Jason on foot until
    # its next drawing, chr's draws the poked 'Mech at once.
    {"name": "chain_mech_poked", "keys": MECH_POKES + " idle:1", "match": {}},
    {"name": "chain_mech_walk", "keys": "left*29", "match": SCREEN},
    {"name": "chain_mech_enc", "keys": "left", "match": SCREEN},
    {"name": "chain_mech_round_1", "keys": "y y enter arm:1 n", "match": {}, "fight": True},
    {"name": "chain_mech_end_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_mech_end_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_mech_end_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_mech_loot", "keys": "enter", "match": SCREEN},
    {"name": "chain_mech_swap", "keys": "enter", "match": SCREEN},
    {"name": "chain_mech_return", "keys": "enter", "match": SCREEN},
]

# The long unpoked run: its own tier (`--campaign`, ctest label campaign).
CAMPAIGN = {"chain", "chain_death", "chain_mech_poked"}

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
             ("course", COURSE_CASES, COURSE_FULL_CASES, []),
             ("spot", [], SPOT_CASES, []),
             ("fourth", [], FOURTH_CASES, []),
             ("raid", [], RAID_CASES, []),
             ("rest", [], REST_CASES, []),
             ("chain", [], CHAIN_CASES, []),
             ("chain_death", [], CHAIN_DEATH_CASES, []),
             ("chain_mech_poked", [], CHAIN_MECH_POKED_CASES, []),
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


CHARACTERS_AT = 0xC614  # F:c614, the 16 character records of 0x11 bytes


def char_poke(token):
    """The original's poke for a char:R:BB=VV token: one byte inside the 16
    character records, nothing else."""
    try:
        record, _, rest = token[len("char:"):].partition(":")
        byte, _, value = rest.partition("=")
        r, b, v = int(record, 16), int(byte, 16), int(value, 16)
    except ValueError:
        raise ValueError(f"bad record poke {token!r}: use char:R:BB=VV (hex)")
    if not (0 <= r < 16 and 0 <= b < 0x11 and 0 <= v <= 0xff):
        raise ValueError(f"refused record poke {token!r}: record 0-f, byte below 11, value one byte")
    return "@poke:F:%04x=%02x" % (CHARACTERS_AT + 0x11 * r + b, v)


def original_token(k):
    """A case's key as the original's script has it: flag: is a poke, and
    orig: marks input that only the original asks for (the training quiz,
    which the rebuild does not show: docs/combat.md)."""
    if k.startswith("flag:"):
        return flag_poke(k)
    if k.startswith("mech:"):
        return mech_poke(k)
    if k.startswith("char:"):
        return char_poke(k)
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
            if k.startswith(("mech:", "char:")):
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
    finished, _ = run.run(a.game_dir, original_script(cases), ref_dir, timeout=2400 if sequence in CAMPAIGN else 900 if a.full else 240)
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
    ap.add_argument("--campaign", action="store_true",
                    help="only the campaign sequences (the long unpoked run), all their cases")
    ap.add_argument("--sequence", action="append", help="run only this sequence (may be repeated)")
    a = ap.parse_args()
    run.stop_on_signals()  # a stopped run leaves no DOSBox-X (or its quit dialog)

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
    # The campaign sequences run only with --campaign (or by name): they
    # would make the everyday --full run too long.
    jobs = [(sequence, quick + (full if a.full or a.campaign else []), chr_options)
            for sequence, quick, full, chr_options in SEQUENCES
            if (sequence in a.sequence if a.sequence else (sequence in CAMPAIGN) == a.campaign)]
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
