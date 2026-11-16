# Comparing the rebuild with the original

`tools/reference/` runs the original game headless with a key script, captures its screen at
marked steps, and compares those captures with the rebuild pixel for pixel. Nothing from the
game goes in the repo: the runs copy the player's own game folder into a scratch drive and
delete it afterwards.

## How the original reads keys

There is one keyboard read in the whole program, `287f:0b8a`: BIOS int 16h function 0 (wait for
a key), giving a character or minus the scan code. It sits behind `273d:0259`, which can also
record keys into a buffer or play them back from one (F:3938, F:458c: a built-in demo or
replay mechanism, not traced further). Whether a key is waiting is asked of DOS: `273d:002f`
calls `287f:3bdc`, int 21h function 0Bh. The program never hooks int 9.

Both kinds of waiting occur:

- **Blocking**: `273d:0259` alone, e.g. the yes/no question (`1000:1a13`).
- **Polling**: the main loop (`1000:002d`) asks whether a key is waiting; if not, it runs its
  idle half (`1000:01f4`, below) and asks again. Some "press a key" waits are polling loops that
  never time out (e.g. 1000:a965).
- **Flushing**: after reading a key the program often empties the buffer (`1000:2a2b`: while a key
  is waiting, read it), so type-ahead is thrown away. A key that ends a polling wait can be eaten
  by the flush that follows, and the program then waits again.

## The key feeder (tools/capture/snap.asm)

`SNAP 0 K` loads KEYS.BIN (written by `run.py`) and hooks int 16h and int 21h. It puts the next
key in the BIOS buffer (int 16h function 5) only when the program is waiting for one:

- int 16h function 0/10h is called and the buffer is empty; or
- int 21h function 0Bh is asked with an empty buffer for the 4th time in a row since the last key
  read. A flush asks once and stops, so it never gets a key; a polling wait asks again and gets one.

`idle:N` in a script lets N more polls go by before the next item (the main loop runs one idle
pass per poll); a blocking read skips it, since nothing idles there. In the town that gives
exact pass counts: the main loop starts with 3 passes before the first item, a key is followed
by 2 (its flush uses up one of the 4 polls), and `idle:N` adds N. `check.py` spells these out
for chr (`chr_script()`), so both sides run the same number of game ticks.

`@arm:N` before a key sets the same count, but when that key is read, and it works in a blocking
read too: the Nth poll after the key gets the next item. It is for a wait that starts right
after a blocking question, where `idle:N` would be skipped (the computer-fought rounds:
`mission` below).

Captures (`@snap:NAME`) are written at the same moments, so they show the screen the program is
waiting on. In the EGA mode a capture also holds the first 640K of memory and the game's data
segment (DS when it last asked DOS whether a key was waiting), so the game's state can be read
back: `tools/reference/memory.py CAPTURE ds:4fc0:3 "[538a]:d370:4"` prints bytes, where
`[538a]` is the segment whose number is the word at DG:538a, as the game reaches its far data.
`state.py CAPTURE` prints the state the rig compares, in the form `chr --state` prints it: the
random numbers, C-Bills, the townspeople's two tick counters (DG:57fe, the town's; DG:5802, a
mission walk's), the townspeople, the party's place, the flags, Jason's skills, the 16
character records and 8 'Mech records byte for byte, the menus' remembered items, the terrain
generator's noise table and the nine area grids (docs/wilderness.md). When the script ends SNAP writes SNAPEND.TXT and `run.py` kills DOSBox-X. Nothing
depends on the clock or on how fast the host is: two runs of the same script give byte-identical
captures (checked for the start-up, the town and the Citadel question).

## Running it

    tools/reference/run.py --game-dir DIR --out OUT "3 3 @snap:note enter @snap:infocom ..."

DOSBox-X runs with `-silent`, no sound, no PC speaker and the dummy SDL audio driver: no window and
no sound, ever, so it is safe while the screen is shared. A run to the town takes about 7 seconds.
Key names are as for `chr --script`, plus `esc`, digits and `@snap:NAME`.

    tools/reference/compare.py OUT/NAME.bin chr-screenshot.bmp --diff diff.png
    tools/reference/check.py --chr build/chr --game-dir DIR --data-dir DIR --work DIR

`check.py` (ctest `reference_compare`) runs the cases in it: each lists the rectangles that must
be identical and the differences still known, and the game state (random numbers, C-Bills, the
townspeople's records) must be identical too. `--full` (ctest `reference_compare_full`, labelled
`long`: `ctest -LE long` leaves it out) adds the long cases. The sequences (below) run side by side, each in its own copy of the game
folder, and their reports are printed in order: about 25 seconds for the quick cases and two
minutes with `--full` on the Mac Studio. A third tier, `--campaign`, runs only the long
unpoked run (`chain`), all its cases, which `--full` leaves out: about four and a half minutes
now, growing with the run. Its ctest `reference_compare_campaign` (label `campaign`) exists only
when configured with `-DCHR_REFERENCE_CAMPAIGN=ON`. `--sequence chain` also runs it. It skips
(exit 77) without DOSBox-X, nasm or the game.

## The start-up, as captured

1. Text mode: "Which graphics adapter do you have?" (3: 256K EGA/VGA), then "How many disk
   drives?" (3: hard disk), then a note about where saved games go; a key goes on. The rebuild
   leaves these to the platform (docs/startup.md).
2. The Infocom/Westwood screen, then the title; a key each. Rig cases `start_infocom`,
   `start_title`.
3. The panel and window 6 with the notice and "Is this your first time playing BattleTech?"
   (`1000:50c8`). Rig case `start_first_time`. Yes goes to the town; No asks whether to load a
   saved game.
4. The town, map 1, at the start position.

The rig drives `chr` through the same start-up (no `--map`), so every case after it runs on from
the rebuilt start-up.

## Test aids: setting a flag in both programs

Some cases need game state that play would take long to reach, or that the rebuild cannot reach
yet (a training mission past the first two, before those missions are rebuilt). For those a case
can set a flag in both programs at the same point of the script: `flag:NN=VV` (hex) in a case's
keys in `check.py`. `mech:R:BB=VV` (hex) likewise writes byte BB of 'Mech record R (F:c724 +
0x7d R + BB), for damage that no case can cause yet. Both check.py and chr refuse anything
outside the eight records (R 0-7, BB below 0x7d, so F:c724-cb0b).

- **The original:** run.py's `@poke:SEG:OFFSET=BYTES` (here `@poke:F:d30c+NN=VV`). SNAP writes the
  bytes into the running copy's memory in DOSBox-X at that point; nothing is written to the
  game's files. SEG is F, P or DG (docs/original.md, "Data segments"), found through the DGROUP
  word the game itself keeps it in. SNAP refuses any segment below the game's first data segment
  (the game's code and the run-time library) or above DGROUP, writes nothing and leaves
  POKEFAIL.TXT, which run.py reports as a failed run. A poke needs the game's DS; it works in
  the town's main loop, which asks DOS for keys, and at the fight's orders menu (the `kick`
  sequence's record poke).
- **The rebuild:** `set:NN=VV` in a `--script`, applied at that point by the town's main loop
  (a question or the start-up refuses one), or `chr --set-flag NN=VV`, applied as the town starts.
  `mech:R:BB=VV` is applied wherever the next key or pass is taken, the fight's reads included.

`chr --window X,Y` (hex, repeatable) prints the cell window built at that place from the terrain
as the script leaves it (docs/wilderness.md). check.py adds it to every state case, at the place
the original last built its own; `window.py CAPTURE...` does the same for any captures with a new
game's terrain.

What a poke may not touch: code, and anything that decides the copy-protection path. Flag 14
(F:d320), which skips the training quiz while nonzero, is refused by run.py and by chr; the
quiz's result is never in memory to poke. A case that uses a poke says so in its name or
comment, with what it stands in for (for example "stands in for passing missions 0-1"). When
the play it stands in for is rebuilt, a long case reaches the same point with keys alone, and
the poked case stays as a quick one.

## Sequences and keys only the original asks for

`check.py` has several sequences, each one run of the original from a new game: `town` (the
cases above) and `training` (training mission 2: the walk to the training center, its greeting,
the briefing, the 'Mech menu, the Locust's walk-in under the first of the fight's three questions,
the other two, and the orders menu with its box on the Locust; it pokes flags 00 and 13, standing in for passing missions 0-1
and resting in the Barracks) and `quiz` (the same walk with `chr --show-quiz`: the message, the
three questions and the greeting after them). A key written `orig:KEY` goes to the original
only: without the scene, the training center's quiz takes a key and three menu answers there
that the rebuild does not ask (docs/combat.md 7). In `quiz`, `orig:` keys move the original's
menu bar onto the right part, where the rebuild's bar starts, before each comparison. A
sequence can give chr options of its own (`SEQUENCES`). `check.py --sequence NAME` runs only
the named sequences.

`mission` is training mission 2 fought by the computer, with no combat messages and no combat
graphics (docs/combat.md 1.2.2). Once the three questions are answered nothing is drawn and no
key is read until the fight ends; the only input is the "is a key waiting?" question the fight
asks once a round (`203b:1015`, which would hand the fight back to the player). `arm:N` before a
key sets the feeder's poll count when that key is read (run.py's `@arm:N`, SNAP's item 0FFFCh),
so the Nth question after it is answered: `arm:1 n` puts the first capture after round 1, and
each `idle:1` after it is one more round. chr asks the same question once a round
(`KeySource::poll`): an idle pass answers no, a key yes, and the end of the script stops the
fight there; `arm:N` gives chr N - 1 passes after the key instead of the usual 2. The quick
cases end after round 1; `--full` adds rounds 2-8 and the end of the fight, then a key: the party
on Jason's 'Mech (17dc:134b) and the mission's walk, with the target's wreck drawn (its mark),
back to the door (a way found with chr's step check), the script's verdict, and the town. `verbose` is the
same fight with combat messages Verbose (graphics still off), so every capture compares the last
shot's line in window 4 too; its quick case is round 1 as well. `brief` (messages Brief) is
long only. `drawn` is the fight with no messages and combat graphics on: the screen at each
round's end shows the last of the shots' graphics; round 1 quick, the rest long. `chr --events`
lists what the logic showed on the way, and `--event N --screenshot F` renders the N-th.
`orders` is the mission fought by the player (Verbose, graphics on): Begin Fight with no orders
each round, each capture at the next round's orders menu (round 1 quick, the rest long), then
Flee. `moves` gives the player's move orders and fights on them (docs/combat.md 2.6).
`waypoints` (long only) gives thirteen walk orders on the unit's own cell, so the thirteenth goes
into figure 1's orders (bug 17), then a round. `weapons` opens Use Weapons, gives the first
weapon the enemy through the target picker and fights four rounds (the picker's view reaches
past the map file's east edge, into the terrain: docs/wilderness.md).
`kick` orders a kick at the enemy through the picker (its view reaches the terrain too) and fights three rounds; the long run then takes the left hip away (a record poke
standing in for a critical hit): Kick cannot, and the round drops the kick still ordered. `scan` opens Scan Unit: Friends (Next comes back to the unit), Done, Enemies (the enemy Locust
and its direction, the view reaching the terrain), Done, Cancel, and a round. `people` is mission 3 (flag 00 = 3): three to six enemy people, the orders menu and 20 rounds
(long; from round 6 the soldiers' lines of sight read the area grids before the cell window), compared with the walker lines showing figures 16-23 during the fight, as the original's
shared memory does. `course` is mission 0 with no poke (no flag set at all, as in a new game): the
Locust walks in and the course starts, three steps, then (long) the way to the southeast corner
and its message, back to the door (found with chr's step check, which the original agrees with
along it), the script's verdict (special 2), and a key: special 3 hands the 'Mech back and the
party is in town on foot, where it walks on. `spot` (long only) goes on from there to mission 1
without a poke: 2900 idle polls let flag 14 run down (255 town ticks, an idle tick every 11
polls), the training center again with the quiz on the original's side, the second mission in the
Wasp, the way to the marked spot (the same random spot in both) and back, and the town after.
The window it compares is the one chr holds, which after the spot's cell is put back still shows
tile 0x50 until the next step, as the original's does. `chain` (campaign tier only) goes on from `spot`,
still without a poke: flag 14 runs down again, the third mission's first try (flag 13 is 0: the
commandant and special 3), the Barracks and back as in `rest` (one step east first, as the
handback leaves the party a cell west of `rest`'s), the quiz again and the briefing, then the third mission in the Wasp with the computer fighting: the shell falls in round 16, the walk back, the verdict (passed) and the town; then mission 3 the same way (the last soldier falls in round 13), passed; then mission 4, where the Locust flees at the first orders menu: the walk back and the verdict (not passed). `fourth` (long only) is mission 4 (flag 00
poked to 4): no computer question (F:0090), the Locust flees at the first orders menu, the walk
back, the verdict and the town. `raid` (long only) is mission 7 (flag 00 poked to 7), the
Kurita raid: the computer question (only mission 4 skips it), then Begin Fight round after round
through the five story lines and the load of map 11, until the Jenners bring the Locust down,
then the results and the script. `rest` (long only) is the third mission's first try with
flag 13 not poked (flags 00 = 2 and 01 = 1 stand in for missions 0-1): the commandant sends
Jason away and takes the 'Mech back (special 3), he walks to the Barracks, sleeps (flag 13 + 1,
the story, flag 14 = 0) and walks back the same way, steps turned round, for the quiz, the
greeting and the briefing. `handover` (long
only) lets the computer fight six rounds, hands the fight back with a key at
the round's poll, and flees and fails. The training
cases at its start are captured but not compared (`"compare": False`): the `training` sequence
compares them. These cases compare the fight's tables too: a case with
`"fight": True` runs `chr --fight-state` and `state.py --fight` (the 24 figures' places,
active and destroyed counts, headings, facings and frames, their orders, targets and paths, what
the figure drawer wrote for each (on screen, terrain, tile, the 'Mechs' tile above and half-row,
screen place), the parked facings, where each is in its script (DG:01f6), the heat, the burning
counts, DG:014a and the map marks).

## Findings

Every case in `check.py` is identical over the whole screen and in the game state:

- quick: the town start, 9 idle passes later, 120 more (a townsperson walking in view), the
  Citadel question;
- full: 250 game ticks to the second money step, a walk west and south through MAP1 (146 steps,
  90 into walls), 400 idle passes, a walk back east and round, then waiting by the Barracks door
  while a townsperson goes in and, 1,200 passes later, comes out;
- training: the walk to the training center (flags poked, above), its greeting, the briefing,
  the 'Mech menu, the walk-in with the fight's three questions, and the orders menu;
- quiz: the same walk with `--show-quiz`, the quiz's message and its three questions over the
  whole screen, and the greeting after it.

docs/town_loop.md has what the main loop does and when, docs/townspeople.md the townspeople.

Found by the comparison along the way:

- The training walk found two rules of the townspeople's step (docs/townspeople.md): the party's
  places are recorded right after its step, before the townspeople move (`1000:0081`), and a
  person's tile is read from the 24x24-cell window around the party, so past its side edge the
  cell of the next or the previous row decides.

- The move log ("North" in the upper box): the main loop prints the direction (DG:01aa) in
  window 4 after each walking key unless a message came up. Once the box is full it scrolls a
  line at a time (each entry is a line break, then the word).
- The main loop's pace is the display's vertical retrace, not the timer (docs/town_loop.md).
- The party's figure is sprite slot 0xa6, not a 'Mech: its frame offset starts at 0x10.
- The random numbers at the town start: 256 + 8 draws before map 1 is loaded, 8 for the
  townspeople, 1 more before the first pass (docs/townspeople.md).
