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

## Findings so far (town start and the Citadel question)

Identical: the map tiles, the panel's frames, the party names, the message box and its text, the
yes/no line, and the move log. Different, with the cause in the original:

| Difference | Cause |
|---|---|
| The party's figure | `1000:051b` draws the figures; not rebuilt yet. Leader at screen (208, 96), as docs/movement.md says. |
| Fence and other animated tiles | `1000:240b`: 10 tiles (numbers at DS:04b0) cycle through 3 frames from ANIMATE.ICN (loaded by `1000:320b`). The main loop's idle half (`1000:01f4`) waits a timer tick per pass and every 11 passes runs a game tick: one animation step, every third tick `1000:24c2`, money, and a redraw. The phase in a capture depends on how many ticks ran, which the feeder makes repeatable. |
| The three bars per character | `1000:4bc1`, drawn by `1000:4aa6` in columns 9-11 under "BDC". |
| "C-Bills:" and the amount | `1e31:1fdf`. The amount is 20 on the first-time screen and 35 in town: the game tick adds to it. |
| The move log ("North" in the upper box) | Found by this comparison and now rebuilt: the main loop prints the direction (DS:01aa) in window 4 after each walking key unless a message came up. |
