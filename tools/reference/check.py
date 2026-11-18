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

# Poked: chain_mech_poked with Jason's hit points also lowered (character 0
# byte 0x0f, our own test value), so the same won fight ends in healing.
# On foot he loses to every group in reach, even with a poked rifle and the
# most hit points a byte allows, and the soldiers never hurt a 'Mech's pilot.
# Pokes draw no numbers, so the fight is the same; only the tail is compared.
CHAIN_HEAL_POKED_CASES = [dict(case, compare=False) for case in CHAIN_MECH_POKED_CASES
                          if case["name"] != "chain_mech_return"]
CHAIN_HEAL_POKED_CASES[[c["name"] for c in CHAIN_HEAL_POKED_CASES].index("chain_mech_poked")] = {
    "name": "chain_heal_poked", "keys": MECH_POKES + " char:0:0f=30 idle:1", "match": {}}
CHAIN_HEAL_POKED_CASES += [
    {"name": "chain_heal", "keys": "enter", "match": SCREEN},
    {"name": "chain_heal_return", "keys": "enter", "match": SCREEN},
]

# Poked: chain_mech_poked with Jason given Tech 4 (character 0 byte 9, our
# own test value). After the soldiers he walks on to a Wasp and a soldier;
# the soldier runs, the computer brings the Wasp down, and the repairs run
# (no scrap money: Jason is character 0, bug 24). Only the tail is compared.
CHAIN_REPAIR_POKED_CASES = [dict(case, compare=False) for case in CHAIN_MECH_POKED_CASES]
CHAIN_REPAIR_POKED_CASES[[c["name"] for c in CHAIN_REPAIR_POKED_CASES].index("chain_mech_poked")] = {
    "name": "chain_repair_poked", "keys": MECH_POKES + " char:0:09=04 idle:1", "match": {}}
CHAIN_REPAIR_POKED_CASES += [
    {"name": "chain_repair_walk", "keys": "enter", "match": SCREEN, "compare": False},
    {"name": "chain_repair_enc", "keys": "left*27", "match": SCREEN},
    # The key after round 1's start hands the fight back to the player
    # (203b:1015); each Begin Fight (Enter, the default) then runs a round
    # with the computer still moving the Chameleon. Round 1 ends with "Some
    # of the enemy humans have fled!" (2.10) and a key. The Wasp falls in
    # the round of the twelfth key; the next shows the scuffle box.
    {"name": "chain_repair_round_1", "keys": "y y enter arm:1 n", "match": {}, "fight": True},
    {"name": "chain_repair_player", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_repair_fled", "keys": "enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_repair_key_%d" % n, "keys": "enter", "match": {}, "fight": True} for n in range(3, 12)] + [
    {"name": "chain_repair_key_12", "keys": "enter", "match": SCREEN},
    {"name": "chain_repair_over", "keys": "enter", "match": SCREEN},
    {"name": "chain_repair", "keys": "enter", "match": SCREEN},
    {"name": "chain_repair_return", "keys": "enter", "match": SCREEN},
    {"name": "chain_repair_back", "keys": "enter", "match": SCREEN},
]

# Poked: chain_mech_poked, then after the soldiers a second party member,
# Rex (character 1: name number 1, attributes, Piloting 2, on foot, hit
# points; our own test values), and the walk on to the same Wasp and
# soldier as chain_repair_poked. Rex fights on foot as figure 5. After the
# fight he is a pilot without a 'Mech: Yes to salvage, and Jason, the only
# one with Tech, salvages the Wasp into 'Mech record 1 for him (15ab:04f9).
# Only the tail is compared.
SALVAGE_POKES = " ".join(["char:1:00=01", "char:1:01=08", "char:1:02=09", "char:1:03=07",
                          "char:1:08=02", "char:1:0c=08", "char:1:0f=50"])
CHAIN_SALVAGE_POKED_CASES = [dict(case, compare=False) for case in CHAIN_MECH_POKED_CASES] + [
    {"name": "chain_salvage_walk", "keys": "enter", "match": SCREEN, "compare": False},
    {"name": "chain_salvage_poked", "keys": SALVAGE_POKES + " idle:1", "match": {}},
    {"name": "chain_salvage_enc", "keys": "left*27", "match": SCREEN},
    # As in chain_repair_poked: a key hands the fight back, then Begin
    # Fight each round. With Rex on foot the scuffle box comes three rounds
    # later.
    {"name": "chain_salvage_round_1", "keys": "y y enter arm:1 n", "match": {}, "fight": True},
    {"name": "chain_salvage_player", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_salvage_fled", "keys": "enter", "match": SCREEN, "fight": True},
] + [{"name": "chain_salvage_key_%d" % n, "keys": "enter", "match": {}, "fight": True} for n in range(3, 16)] + [
    {"name": "chain_salvage_over", "keys": "enter", "match": SCREEN},
    {"name": "chain_salvage_question", "keys": "enter", "match": SCREEN},
    {"name": "chain_salvage", "keys": "enter", "match": SCREEN},
    {"name": "chain_salvage_none", "keys": "enter", "match": SCREEN},
    {"name": "chain_salvage_back", "keys": "enter", "match": SCREEN},
]

# Poked: chain_salvage_poked's fight up to the hand-back, then the orders
# for Rex on foot (figure 5): the person menu (DG:3abc) and its orders.
# Only the tail is compared.
CHAIN_ORDERS_POKED_CASES = [dict(case, compare=False) for case in
                            CHAIN_SALVAGE_POKED_CASES[:[c["name"] for c in CHAIN_SALVAGE_POKED_CASES].index("chain_salvage_player") + 1]] + [
    {"name": "chain_orders_rex", "keys": "up*2 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_weapon", "keys": "up*3 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_weapon_2", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_weapon_3", "keys": "enter", "match": SCREEN, "fight": True},
    # Scan Unit: Friends (Jason's Chameleon, then Rex's condition, weapon
    # and armour), Done; Enemies ("humans only" while the Wasp stands, a
    # key, then the enemy person), Done.
    {"name": "chain_orders_scan", "keys": "down*2 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_friends", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_friends_rex", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_friends_back", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_friends_done", "keys": "down*2 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_scan_2", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_enemies", "keys": "down enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_enemy_human", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_enemies_done", "keys": "down enter", "match": SCREEN, "fight": True},
    # Clear Moves, Computer (the plan shown again), Move two cells left and
    # accept, then Begin Fight and two rounds.
    {"name": "chain_orders_clear", "keys": "up*3 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_computer", "keys": "down*2 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_move", "keys": "up*3 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_move_left", "keys": "left*2", "match": SCREEN, "fight": True},
    {"name": "chain_orders_moved", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_begin", "keys": "down*7 enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_round_2", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_orders_round_3", "keys": "enter", "match": SCREEN, "fight": True},
]

# The unpoked chain on from the ruined town (docs/scripts.md, "After the
# raid"): branched at chain_raid_key_57 so chain_death and the poked
# sequences keep their prefix. With no idle there (N = 0 in the sweep of
# 0-255 idle passes) the walk out is the chain's own, so those cases are
# shared. Jason, alone on foot, flees every encounter in reach (2.7: three
# in four escape) on the way to map 2 (area 0x8a). Built a segment at a time.
CHAIN_TOWN_CASES = [dict(case, compare=False) for case in
                    CHAIN_CASES[:[c["name"] for c in CHAIN_CASES].index("chain_out_area") + 1]] + [
    # Segment 1: chain_death's encounter (four soldiers in reach), declined,
    # the computer not fighting, no messages, no graphics, then Flee from the
    # orders menu: eluded, a key, and the party back where it was.
    {"name": "chain_town_enc", "keys": "home", "match": SCREEN},
    {"name": "chain_town_enc_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_computer", "keys": "n", "match": SCREEN},
    {"name": "chain_town_messages", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_graphics", "keys": "n", "match": SCREEN},
    {"name": "chain_town_menu", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_orders", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_flee", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_back_2", "keys": "enter", "match": SCREEN},
    # Segment 2: 16 steps north-west (up-left and up in turn) to a group of
    # five in reach; declining it evades them this time (2.12): the escape,
    # its key and the return.
    {"name": "chain_town_walk_2", "keys": "home up " * 7 + "home up", "match": SCREEN},
    {"name": "chain_town_2_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_2_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_2_back", "keys": "enter", "match": SCREEN},
    # Segment 3: 64 steps west and north-west (left and up-left in turn),
    # with no encounter in reach; in halves.
    {"name": "chain_town_walk_3", "keys": "left home " * 15 + "left home", "match": SCREEN},
    {"name": "chain_town_walk_3b", "keys": "left home " * 15 + "left home", "match": SCREEN},
    # Segments 4 and 5: up-left to a group in reach (five, then three),
    # declined and evaded each time.
    {"name": "chain_town_walk_4", "keys": "home*9", "match": SCREEN},
    {"name": "chain_town_4_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_4_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_4_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_5", "keys": "home*27", "match": SCREEN},
    {"name": "chain_town_5_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_5_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_5_back", "keys": "enter", "match": SCREEN},
    # Segment 6: 51 steps up-left into area 0xaa, where map 4 (area 0xb9)
    # comes into the grids (docs/wilderness.md, the map loads).
    {"name": "chain_town_walk_6", "keys": "home*26", "match": SCREEN},
    {"name": "chain_town_walk_6b", "keys": "home*25", "match": SCREEN},
    # Segments 7-9: on up-left, then north, to groups of six, two and four
    # in reach, each declined and evaded.
    {"name": "chain_town_walk_7", "keys": "home*30", "match": SCREEN},
    {"name": "chain_town_walk_7b", "keys": "home*31", "match": SCREEN},
    {"name": "chain_town_7_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_7_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_7_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_8", "keys": "up home " * 4 + "up", "match": SCREEN},
    {"name": "chain_town_8_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_8_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_8_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_9", "keys": "up*28", "match": SCREEN},
    {"name": "chain_town_9_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_9_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_9_back", "keys": "enter", "match": SCREEN},
    # Segment 10: 12 steps north, until map 2 (area 0x8a) is loaded into
    # the grids.
    {"name": "chain_town_walk_10", "keys": "up*12", "match": SCREEN},
    # Segments 11-14: map 2 is walled on its south and east sides; on foot
    # (tiles from 0x55 block) its only way in is the west side at rows
    # 27-31, which leads to one door, entrance 10 (the 'Mech Garage). North
    # to the wall, west along it and round (the planner's walks, some steps
    # blocked), then north up the west side to the door: one group evaded,
    # one fled, one evaded on the way. Ends at the door's question.
    {"name": "chain_town_walk_11", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_11b", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_11c", "keys": "up*17", "match": SCREEN},
    {"name": "chain_town_11_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_11_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_11_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_12", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_12b", "keys": "up*8 home left home left home left home left home left home left home left home left home left home left home left home left", "match": SCREEN},
    {"name": "chain_town_walk_12c", "keys": "home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left", "match": SCREEN},
    {"name": "chain_town_walk_12d", "keys": "home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left", "match": SCREEN},
    {"name": "chain_town_walk_12e", "keys": "home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left home left", "match": SCREEN},
    {"name": "chain_town_walk_12f", "keys": "home left home left home left home left right up right up right up right up right up right up right up right up right up right up right up right up", "match": SCREEN},
    {"name": "chain_town_walk_12g", "keys": "right up right up right up right up right up right up right up right up right up right up right up right up right up right up right up right up", "match": SCREEN},
    {"name": "chain_town_walk_12h", "keys": "right up right up right up right up left*4 up*11", "match": SCREEN},
    {"name": "chain_town_12_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_12_computer", "keys": "n", "match": SCREEN},
    {"name": "chain_town_12_messages", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_12_graphics", "keys": "n", "match": SCREEN},
    {"name": "chain_town_12_menu", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_12_orders", "keys": "enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_12_flee", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_town_12_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_12_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_12_back_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_13", "keys": "up*2", "match": SCREEN},
    {"name": "chain_town_13_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_13_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_13_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_14", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_14b", "keys": "up right*8", "match": SCREEN},
    # The garage (GARAGE.BLD) on foot: special 11 finds no 'Mech records in
    # use, so flag 18 is set and the party is put inside, at the rear exit.
    {"name": "chain_town_garage", "keys": "y", "match": SCREEN},
    {"name": "chain_town_garage_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_garage_out", "keys": "up", "match": SCREEN},
    {"name": "chain_town_garage_out_2", "keys": "up", "match": SCREEN},
    # Time in town before setting out: outdoors only steps draw, so the walk
    # to map 3 meets a group Jason can neither evade nor flee (twice) at a
    # fixed draw; 1280 or more idle passes here use that draw up in town
    # (swept 0-2500; docs/reference.md). 1300 for a margin.
    {"name": "chain_town_wait", "keys": "idle:1300", "match": SCREEN},
    # Out by the garage's front door (entrance 0) towards map 3's jail, the
    # recruit open to a party of one (docs/scripts.md): with flag 18 set,
    # special 1d finds no 'Mech held, so flag 18 is cleared and the party is
    # put outside the front door.
    {"name": "chain_town_to_garage", "keys": "down down left left left", "match": SCREEN},
    {"name": "chain_town_garage_door", "keys": "left", "match": SCREEN},
    {"name": "chain_town_garage_2", "keys": "y", "match": SCREEN},
    {"name": "chain_town_garage_2_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_outside", "keys": "left", "match": SCREEN},
    # Segments 15-17: north into the row of map 3's area, then east along
    # it towards the jail; two groups evaded.
    {"name": "chain_town_walk_15", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_15b", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_15c", "keys": "up*32", "match": SCREEN},
    {"name": "chain_town_walk_15d", "keys": "up", "match": SCREEN},
    {"name": "chain_town_15_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_15_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_15_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_16", "keys": "pgup*32", "match": SCREEN},
    {"name": "chain_town_walk_16b", "keys": "pgup*32", "match": SCREEN},
    {"name": "chain_town_walk_16c", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_16d", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_16e", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_16f", "keys": "right*14", "match": SCREEN},
    {"name": "chain_town_16_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_16_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_16_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_town_walk_17", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_17b", "keys": "right*32", "match": SCREEN},
    # Segment 18: east into map 3. Most step patterns meet three soldiers
    # in reach 37 steps in (neither evaded nor fled); right, right, up-right
    # is in among the buildings by then and comes to the Armor Shop's door
    # instead. No there, then inside the town to the jail's door (entrance 6).
    {"name": "chain_town_walk_18", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_18b", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_18c", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_18d", "keys": "right*32", "match": SCREEN},
    {"name": "chain_town_walk_18e", "keys": "right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2", "match": SCREEN},
    {"name": "chain_town_walk_18f", "keys": "pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup right*2 pgup", "match": SCREEN},
    {"name": "chain_town_armor_no", "keys": "n", "match": SCREEN},
    {"name": "chain_town_to_jail", "keys": "left*2 end*8 down*5 end*7", "match": SCREEN},
    {"name": "chain_town_jail_door", "keys": "end", "match": SCREEN},
    {"name": "chain_town_jail", "keys": "y", "match": SCREEN},
]

# From chain_town_garage_out_2 (inside map 2, flag 18 set): the first
# companion (docs/scripts.md, "The first companion"). Three groups evaded on
# the way to CLOTHES, where Jason buys clothes (flag 1e); PARTY's first visit
# sets flag 1c and the timer, flag 1d = ff. The timer drops by one a game
# tick, one per 11 idle passes: in chr it reaches 0 after 2792 passes from
# chain_party_party_out (it does not depend on the random numbers); 2800 for
# a margin. The ticks' encounter checks fire too (map 2 counts as outdoors):
# every group up to 3000 passes is out of reach. Then off the door and back
# on, and PARTY's scene up to special 1e (not rebuilt yet: it makes the
# companion, then a fight of kind -1).
CHAIN_PARTY_CASES = [dict(case, compare=False) for case in
                     CHAIN_TOWN_CASES[:[c["name"] for c in CHAIN_TOWN_CASES].index("chain_town_garage_out_2") + 1]] + [
    {"name": "chain_party_walk", "keys": "down pgdn*4 right*52 pgdn*8 right*2", "match": SCREEN},
    {"name": "chain_party_1_no", "keys": "n", "match": SCREEN},
    {"name": "chain_party_1_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_1_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_walk_2", "keys": "right", "match": SCREEN},
    {"name": "chain_party_2_no", "keys": "n", "match": SCREEN},
    {"name": "chain_party_2_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_2_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_walk_3", "keys": "right", "match": SCREEN},
    {"name": "chain_party_3_no", "keys": "n", "match": SCREEN},
    {"name": "chain_party_3_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_3_back", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_clothes_door", "keys": "right", "match": SCREEN},
    {"name": "chain_party_clothes", "keys": "y", "match": SCREEN},
    {"name": "chain_party_clothes_buy", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_clothes_out", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_to_party", "keys": "down pgdn*3 down*6 pgdn*6 down*4 pgdn*8 right*7", "match": SCREEN},
    {"name": "chain_party_party", "keys": "y", "match": SCREEN},
    {"name": "chain_party_party_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_party_key_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_party_out", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait", "keys": "idle:2800", "match": SCREEN},
    {"name": "chain_party_off", "keys": "left", "match": SCREEN},
    {"name": "chain_party_door_2", "keys": "right", "match": SCREEN},
    {"name": "chain_party_party_2", "keys": "y", "match": SCREEN},
    {"name": "chain_party_scene_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_4", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_5", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_6", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_7", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_8", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_9", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_10", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_11", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_12", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_scene_13", "keys": "enter", "match": SCREEN},
    # Special 1e: the companion (record 1), the soldiers, then the fight
    # (kind -1) at its first question (docs/combat.md 1.6).
    {"name": "chain_party_member", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_member_2", "keys": "enter", "match": SCREEN},
    # Messages as they were, no graphics, then Flee from the first orders
    # menu: kind -1 may flee (combat.md 1.6), and this draw escapes. Jason
    # and the companion are walked back to the alley's entry position.
    {"name": "chain_party_messages", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_graphics", "keys": "n", "match": SCREEN},
    {"name": "chain_party_flee", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_party_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back", "keys": "enter", "match": SCREEN},
    # West to REPAIR (entrance 3; the door is entered from its own row,
    # one below the entrance's), whose first recruit needs a member and
    # flag 18 (docs/scripts.md): new_member 5. The recruit's wait draws a
    # number for each poll, then its key is flushed and another read. It
    # starts right after the script's blocking read, with no flush between:
    # the feeder's key comes on the 4th poll, so 3 draws (arm:4).
    {"name": "chain_party_to_repair", "keys": "left*19 home*7 left*8 home*10 left*4", "match": SCREEN},
    {"name": "chain_party_repair_door", "keys": "left*4 home*2 left*28 home*6 up left pgup up right home right*2", "match": SCREEN},
    {"name": "chain_party_repair", "keys": "y", "match": SCREEN},
    {"name": "chain_party_repair_tech", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_recruit", "keys": "arm:4 enter", "match": SCREEN},
    {"name": "chain_party_recruit_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_recruit_key_2", "keys": "enter", "match": SCREEN},
    # Not the apprentice course (specials 0f and 10, not rebuilt): leave.
    {"name": "chain_party_repair_leave", "keys": "up", "match": SCREEN},
    {"name": "chain_party_repair_out", "keys": "enter", "match": SCREEN},
    # To the garage (entrance 0): with the Commando held, leaving costs 150
    # and the party has 135 (docs/scripts.md, the garage fee).
    {"name": "chain_party_to_garage", "keys": "right pgdn right*4 pgup up right up pgup up*7 home*10", "match": SCREEN},
    {"name": "chain_party_garage", "keys": "y", "match": SCREEN},
    {"name": "chain_party_garage_fee", "keys": "y", "match": SCREEN},
    {"name": "chain_party_garage_out", "keys": "enter", "match": SCREEN},
    # Money for the fee: wait at the garage door (flag 24 is 7f here: an
    # encounter can come one tick in 128). The planner's bounded search
    # (docs/reference.md) evades five groups in reach and lets the computer
    # fight the sixth, two soldiers: all three live, and the loot leaves 167.
    # Two weapons are found: the first to Jason (his cudgel, weapon 0, ends
    # the offer), the second to Edward (Jason again would hand the first back).
    {"name": "chain_party_wait_1", "keys": "idle:4970", "match": SCREEN},
    {"name": "chain_party_evade_1", "keys": "n", "match": SCREEN},
    {"name": "chain_party_eluded_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait_2", "keys": "idle:1989", "match": SCREEN},
    {"name": "chain_party_evade_2", "keys": "n", "match": SCREEN},
    {"name": "chain_party_eluded_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait_3", "keys": "idle:14188", "match": SCREEN},
    {"name": "chain_party_evade_3", "keys": "n", "match": SCREEN},
    {"name": "chain_party_eluded_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait_4", "keys": "idle:9", "match": SCREEN},
    {"name": "chain_party_evade_4", "keys": "n", "match": SCREEN},
    {"name": "chain_party_eluded_4", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back_4", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait_5", "keys": "idle:6356", "match": SCREEN},
    {"name": "chain_party_evade_5", "keys": "n", "match": SCREEN},
    {"name": "chain_party_eluded_5", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_back_5", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_wait_fight", "keys": "idle:6840", "match": SCREEN},
    {"name": "chain_party_fight", "keys": "y y enter arm:1 n", "match": {}, "fight": True},
]  + [{"name": "chain_party_round_%d" % n, "keys": "idle:1", "match": {}, "fight": True} for n in range(1, 11)] + [
    {"name": "chain_party_won", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_pockets", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_offer_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_offer_2", "keys": "down down enter", "match": SCREEN},
    {"name": "chain_party_healing", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_return", "keys": "enter", "match": SCREEN},
    # The fee paid: flag 18 clear, the party outside at 0a00,8039, then
    # special 2e gives the 'Mechs back and the crew screen (docs/scripts.md).
    # Rex already pilots the Commando, so Done (the last item) ends it.
    {"name": "chain_party_fee_door", "keys": "down up", "match": SCREEN},
    {"name": "chain_party_fee", "keys": "y", "match": SCREEN},
    {"name": "chain_party_fee_pay", "keys": "y", "match": SCREEN},
    {"name": "chain_party_crew", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_bar", "keys": "up", "match": SCREEN},
    {"name": "chain_party_crew_done", "keys": "enter", "match": SCREEN},
    # With the Commando, out to map 3's Hospital (entrance 1, 0d11,700b): the
    # on-foot route's first step north is blocked for the 'Mech's figure, so
    # the planner's search (docs/reference.md) goes by that route's
    # positions, evading four groups, and ends at the Hospital's question.
    {"name": "chain_party_map3_walk_1", "keys": "home left home*10", "match": SCREEN},
    {"name": "chain_party_map3_no_1", "keys": "n", "match": SCREEN},
    {"name": "chain_party_map3_eluded_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_back_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_walk_2", "keys": "home*38 pgup*109 pgdn*45", "match": SCREEN},
    {"name": "chain_party_map3_no_2", "keys": "n", "match": SCREEN},
    {"name": "chain_party_map3_eluded_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_back_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_walk_3", "keys": "pgdn*10 pgup*55 pgdn*96 pgup*14", "match": SCREEN},
    {"name": "chain_party_map3_no_3", "keys": "n", "match": SCREEN},
    {"name": "chain_party_map3_eluded_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_back_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_walk_4", "keys": "pgup*27", "match": SCREEN},
    {"name": "chain_party_map3_no_4", "keys": "n", "match": SCREEN},
    {"name": "chain_party_map3_eluded_4", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_back_4", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_map3_walk_5", "keys": "pgup*55 pgdn*11 pgup*21 pgdn pgup*2 right*5 pgup*2 home", "match": SCREEN},
    # The Hospital: Search medical records (the third item), the nurse and the
    # doctor, then new_member 6 (docs/scripts.md): its wait follows the
    # scene's blocking read, so arm:4 as at REPAIR. Then leave (the last item).
    {"name": "chain_party_hospital", "keys": "y", "match": SCREEN},
    {"name": "chain_party_hospital_records", "keys": "down down", "match": SCREEN},
    {"name": "chain_party_hospital_nurse", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_hospital_doctor", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_hospital_recruit", "keys": "arm:4 enter", "match": SCREEN},
    {"name": "chain_party_hospital_recruit_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_hospital_recruit_key_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_hospital_leave", "keys": "up", "match": SCREEN},
    {"name": "chain_party_hospital_out", "keys": "enter", "match": SCREEN},
    # On to the jail (entrance 6): ask about Rex's friend (the jailkeeper's menu,
    # item 07dc), the scenes, new_member 4 (arm:4 after the scene's read), and
    # No at the impounded 'Mech's question: Yes runs special 0x28, mode 9.
    {"name": "chain_party_to_jail", "keys": "down*13 end*7", "match": SCREEN},
    {"name": "chain_party_jail", "keys": "y", "match": SCREEN},
    {"name": "chain_party_jail_ask", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_keeper", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_cubicle", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_rick", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_rick_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_recruit", "keys": "arm:4 enter", "match": SCREEN},
    {"name": "chain_party_jail_recruit_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_holocard", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_keys", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_barrier", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_alarm", "keys": "enter", "match": SCREEN},
    # Yes at the impounded 'Mech's question (the only chance: docs/scripts.md,
    # "JAIL after its recruit"): the jail break, mode 9 (docs/combat.md 1.7).
    # Jason alone walks to the bays on row 702d before the first fight (the
    # 81st tick): the first two won't start, the third does. The escape, then
    # the crew screen with everyone on foot: Jason pilots the STINGER, Rex the
    # COMMANDO, the rest on foot.
    {"name": "chain_party_jail_yes", "keys": "y", "match": SCREEN},
    {"name": "chain_party_jail_gunfire", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_break", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_walk_1", "keys": "pgdn*4", "match": SCREEN},
    {"name": "chain_party_jail_walk_2", "keys": "down*5", "match": SCREEN},
    {"name": "chain_party_jail_bay_1", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_bay_1_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_walk_3", "keys": "right*4", "match": SCREEN},
    {"name": "chain_party_jail_bay_2", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_bay_2_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_walk_4", "keys": "right*4", "match": SCREEN},
    {"name": "chain_party_jail_bay_3", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_escape", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_jail_crew", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_jason", "keys": "up up up", "match": SCREEN},
    {"name": "chain_party_crew_jason_seat", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_jason_stinger", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_rex", "keys": "down", "match": SCREEN},
    {"name": "chain_party_crew_rex_seat", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_rex_commando", "keys": "up", "match": SCREEN},
    {"name": "chain_party_crew_rex_pick", "keys": "enter", "match": SCREEN},
    {"name": "chain_party_crew_to_done", "keys": "down*4", "match": SCREEN},
    {"name": "chain_party_crew_done_2", "keys": "enter", "match": SCREEN},
]

# The jail's other answer, its own sequence: No at the impounded 'Mech's
# question gives the 'Mech up for good (docs/scripts.md, "JAIL after its
# recruit"); the script ends and the party is outside.
CHAIN_JAIL_NO_CASES = [dict(case, compare=False) for case in
                       CHAIN_PARTY_CASES[:[c["name"] for c in CHAIN_PARTY_CASES].index("chain_party_jail_alarm") + 1]] + [
    {"name": "chain_jail_no", "keys": "n", "match": SCREEN},
    {"name": "chain_jail_no_out", "keys": "enter", "match": SCREEN},
]

# The jail break's first fight, its own sequence: from the start of mode 9,
# 80 steps off the bays' row (each key is a tick), then the 81st brings a
# fight of kind 3, eight people against Jason alone (docs/combat.md 1.7).
# The computer fights. On the original Jason falls in this first round
# (DG:014a = 0, his record emptied), as in chr; then one pass, the
# townspeople back and walking.
CHAIN_JAIL_FIGHT_CASES = [dict(case, compare=False) for case in
                          CHAIN_PARTY_CASES[:[c["name"] for c in CHAIN_PARTY_CASES].index("chain_party_jail_break") + 1]] + [
    {"name": "chain_jail_fight_steps", "keys": " ".join(["up down"] * 40), "match": SCREEN},
    {"name": "chain_jail_fight", "keys": "up", "match": SCREEN},
    {"name": "chain_jail_fight_round_1", "keys": "y enter arm:1 n", "match": {}, "fight": True},
    {"name": "chain_jail_fight_round_2", "keys": "idle:1", "match": SCREEN, "fight": True},
]

# The main chain from the jail on (chain_party is its prefix, with Rick on
# foot): the double agent found out, its own sequence: on the jail break's crew
# screen Rick is seated as the COMMANDO's rider, which shows his tell and sets
# F:374a (docs/scripts.md). The first random encounter after it (one person,
# 119 passes out) is fought by the computer; after round 1 he acts from the
# rider's seat (the box; docs/combat.md 2.9). Round 1 without arm:1, so the
# box's key is not fed to 273d:086a's flush; the box's key with arm:2, so both
# programs take the next key at the same poll (docs/reference.md).
CHAIN_AGENT_CASES = [dict(case, compare=False) for case in
                     CHAIN_PARTY_CASES[:[c["name"] for c in CHAIN_PARTY_CASES].index("chain_party_crew_rex_pick") + 1]] + [
    {"name": "chain_agent_rick", "keys": "down*3", "match": SCREEN},
    {"name": "chain_agent_vehicle", "keys": "enter", "match": SCREEN},
    {"name": "chain_agent_commando", "keys": "enter", "match": SCREEN},
    {"name": "chain_agent_tell", "keys": "n", "match": SCREEN},
    {"name": "chain_agent_tell_key", "keys": "enter", "match": SCREEN},
    {"name": "chain_agent_done", "keys": "down enter", "match": SCREEN},
    {"name": "chain_agent_encounter", "keys": "idle:119", "match": SCREEN},
    {"name": "chain_agent_fight", "keys": "y y enter n", "match": {}, "fight": True},
    {"name": "chain_agent_acted", "keys": "arm:2 enter", "match": {}, "fight": True},
] + [{"name": "chain_agent_key_%d" % n, "keys": "enter", "match": {}, "fight": True} for n in range(2, 6)] + [
    # Nobody can reach the last soldier past the buildings, so the party
    # flees from the orders menu: escaped, and back in town with Rick gone
    # (flag 27 = 0, flag 24 = 0x7f, his record empty). The same soldier is
    # in reach again straight after.
    {"name": "chain_agent_flee", "keys": "up enter", "match": SCREEN, "fight": True},
    {"name": "chain_agent_eluded", "keys": "enter", "match": SCREEN},
    {"name": "chain_agent_back", "keys": "enter", "match": SCREEN},
]


# The long unpoked run: its own tier (`--campaign`, ctest label campaign).
CAMPAIGN = {"chain", "chain_death", "chain_mech_poked", "chain_heal_poked", "chain_repair_poked",
            "chain_salvage_poked", "chain_orders_poked", "chain_town", "chain_party", "chain_jail_no", "chain_jail_fight", "chain_agent"}

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
             ("chain_heal_poked", [], CHAIN_HEAL_POKED_CASES, []),
             ("chain_repair_poked", [], CHAIN_REPAIR_POKED_CASES, []),
             ("chain_salvage_poked", [], CHAIN_SALVAGE_POKED_CASES, []),
             ("chain_orders_poked", [], CHAIN_ORDERS_POKED_CASES, []),
             ("chain_town", [], CHAIN_TOWN_CASES, []),
             ("chain_party", [], CHAIN_PARTY_CASES, []),
             ("chain_jail_no", [], CHAIN_JAIL_NO_CASES, []),
             ("chain_jail_fight", [], CHAIN_JAIL_FIGHT_CASES, []),
             ("chain_agent", [], CHAIN_AGENT_CASES, []),
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
