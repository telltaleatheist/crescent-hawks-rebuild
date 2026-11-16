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

Captures (`@snap:NAME`) are written at the same moments, so they show the screen the program is
waiting on. In the EGA mode a capture also holds the first 640K of memory and the game's data
segment (DS when it last asked DOS whether a key was waiting), so the game's state can be read
back: `tools/reference/memory.py CAPTURE ds:4fc0:3 "[538a]:d370:4"` prints bytes, where
`[538a]` is the segment whose number is the word at DG:538a, as the game reaches its far data.
`state.py CAPTURE` prints the state the rig compares, in the form `chr --state` prints it. When the script ends SNAP writes SNAPEND.TXT and `run.py` kills DOSBox-X. Nothing
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
`long`: `ctest -LE long` leaves it out) adds the long cases, about a minute and a half. It skips
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
keys in `check.py`.

- **The original:** run.py's `@poke:SEG:OFFSET=BYTES` (here `@poke:F:d30c+NN=VV`). SNAP writes the
  bytes into the running copy's memory in DOSBox-X at that point; nothing is written to the
  game's files. SEG is F, P or DG (docs/original.md, "Data segments"), found through the DGROUP
  word the game itself keeps it in. SNAP refuses any segment below the game's first data segment
  (the game's code and the run-time library) or above DGROUP, writes nothing and leaves
  POKEFAIL.TXT, which run.py reports as a failed run. A poke needs the game's DS, so it works in
  the town's main loop, which asks DOS for keys.
- **The rebuild:** `set:NN=VV` in a `--script`, applied at that point by the town's main loop
  (a question or the start-up refuses one), or `chr --set-flag NN=VV`, applied as the town starts.

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
sequence can give chr options of its own (`SEQUENCES`).

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
