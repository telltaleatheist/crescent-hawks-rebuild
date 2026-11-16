# Comparing the rebuild with the original

`tools/reference/` runs the original game headless with a key script, captures its screen at
marked steps, and compares those captures with the rebuild pixel for pixel. Nothing from the
game goes in the repo: the runs copy the player's own game folder into a scratch drive and
delete it afterwards.

## How the original reads keys

There is one keyboard read in the whole program, `287f:0b8a`: BIOS int 16h function 0 (wait for
a key), giving a character or minus the scan code. It sits behind `273d:0259`, which can also
record keys into a buffer or play them back from one (DS:3938, DS:458c: a built-in demo or
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
waiting on. When the script ends SNAP writes SNAPEND.TXT and `run.py` kills DOSBox-X. Nothing
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
be identical and the differences still known. It skips (exit 77) without DOSBox-X, nasm or the
game.

## The start-up, as captured

1. Text mode: "Which graphics adapter do you have?" (3: 256K EGA/VGA), then "How many disk
   drives?" (3: hard disk), then a note about where saved games go; a key goes on.
2. The Infocom/Westwood screen, then the title; a key each.
3. The panel and window 6 with the copyright text and "Is this your first time playing
   BattleTech?" (`1000:50c8`). Yes goes to the town; No asks "You can load a previously saved
   game, or start a new game. Do you want to load an old game?".
4. The town, map 1, at the start position.

## Findings

All four cases in `check.py` (town start, 9 idle passes later, the Citadel question, and 250
game ticks after answering no) are identical over the whole screen: map, animated tiles, the
party's figure, the panel with its bars, C-Bills and move log. docs/town_loop.md has what the
main loop does and when.

Found by the comparison along the way:

- The move log ("North" in the upper box): the main loop prints the direction (DS:01aa) in
  window 4 after each walking key unless a message came up.
- The main loop's pace is the display's vertical retrace, not the timer (docs/town_loop.md).
- The party's figure is sprite slot 0xa6, not a 'Mech: its frame offset starts at 0x10.
