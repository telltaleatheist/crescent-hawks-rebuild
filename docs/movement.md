# Walking on a map

How the party walks around a town or area map, from `1000:218f` (the walking keys) and
`1000:1c12` (the step check). Rebuilt in `src/game/movement.{h,cpp}`, driven turn by turn by
`src/game/town.{h,cpp}`.

## Positions

A position is two 16-bit words (docs/formats/mtp.md). The low 7 bits of each count half-cells
(8 pixels) inside a 64x64-cell area. The x word's high byte is the area's column (0-15); the y
word's high byte is the area's row times 16. The new game starts at x 0x0c45, y 0xc019: half-cell
(69, 25) of area column 12, row 12, just south of the Citadel on map 1.

The view's top-left corner is 12 half-cells up and left of the position, so the party stands near
the middle of the 27x25 half-cell view and the view scrolls with every step.

## Keys

The program's key reader hands back a character, or minus the scan code for keys without one.
`1000:218f` turns them into a step of one half-cell on each axis it names:

| Key | Scan code | Step |
|---|---|---|
| Up | 0x48 | (0, -1) |
| Down | 0x50 | (0, +1) |
| Left | 0x4b | (-1, 0) |
| Right | 0x4d | (+1, 0) |
| Home | 0x47 | (-1, -1) |
| PgUp | 0x49 | (+1, -1) |
| End | 0x4f | (-1, +1) |
| PgDn | 0x51 | (+1, +1) |

(The keypad's 7, 9, 1 and 3 are Home, PgUp, End and PgDn with Num Lock off.) Every key is one
turn: the main loop (`1000:002d`) reads it, empties the keyboard buffer, selects the upper panel
box (window 4) and takes the step DS:015a times (1 at the start; `1000:3c67` changes it). Unless a
message or question came up during the step (DS:d55c), it then writes the direction's name in
that box ("North", "Northeast", ...; DS:01aa), blocked or not, so the box is a log of the last
eleven steps. The map, the figures and the panel are drawn again afterwards (`1000:01f4`), so a
message drawn over the map lasts until the next key. The keyboard and the idle half of the loop
are in docs/reference.md.

## The step check (1000:1c12)

It returns "blocked" or "free". In order:

1. **Block edges.** If the step crosses into the next 8x8-cell block (16 half-cells) on either
   axis, the program looks at the block it would enter (the diagonal one if both axes cross),
   using the 3x3 block types around the party. Type 0x0f is deep water: it opens the message box
   (`24d3:17c6`), prints "The water is too deep that way." (`24d3:17ea`, which then pauses a
   moment) and the step is blocked. On a town map every neighbouring block is the map's own, so
   this only matters in the open world (not rebuilt yet).
2. **Each character.** The party is up to eight characters, each at a fixed offset from the
   position (in half-cells): (0,0), (0,-1), (0,1), (1,0), (-1,0), (1,-1), (-1,1), (1,1). A new game
   has Jason alone, at (0,0). For each present character, with its position after the step:
   - **Entrance.** Unless a question has been asked this turn already (flag DS:d55c) or the party
     is in its 'Mechs, if the character would stand exactly on one of the map's first 12 entrance
     positions, the message box opens with "Will you enter the" + newline + the place's name +
     "?" and the yes/no question (`1000:1a13`, Yes picked first). Yes goes into the place
     (`17dc:0008`, not rebuilt yet). The check then carries on either way.
   - **Tile.** A figure is drawn on half-cell (x+1, y), so the cell checked is the one holding
     half-cell (x+1, y): cell ((x+1)/2, y/2). If that tile number is at least DS:0150 the step is
     blocked. DS:0150 is 0x55 on every map but map 14, where it is 0x21 (`1000:2da8` sets it).
3. **The 'Mechs** (when the party is in its 'Mechs, slots 0-3 with offsets (0,-2), (0,4), (3,1),
   (-3,1); two cells wide, entrances matched at x +-1, and the tile handlers for map 14's special
   tiles). Not used in town at the start of the game; documented later with the 'Mech maps.

## Moving (1000:218f)

If the step is free, the program moves y first, then x, one half-cell each (`287f:158c` up,
`287f:163b` down, `287f:16e3` left, `287f:17c5` right). If the low byte would leave 0..0x7f the
position moves into the next area: the low byte wraps to 0x7f or 0, the high byte steps by one
area, and the program shifts its grid of loaded areas and loads the maps it now needs between
the two moves. At the edge of the 16x16-area world the position does not change.

## What the rebuild does differently, for now

- Walking off a town map's edge returns `LeftMap` and the party stays put: the original walks on
  into the surrounding area, which needs the world map (not rebuilt yet).
- The deep-water check never fires on a town map, for the reason in step 1; `TooDeep` and its
  message are wired up for when the world is rebuilt.
- Saying yes to an entrance stops the walk (`Town::entered_place`): places are not rebuilt yet.
- Only the party's figures are drawn: the map's objects, the townspeople and the 'Mechs are not
  rebuilt yet (docs/town_loop.md).

## Trying it

    chr --map 1 --script "up*5 y" --screenshot out.bmp

plays the keys headless and prints what each did ("moved at 0c45,c018", "blocked at ...",
"entered place 1 (Citadel)"). Key names: up down left right home end pgup pgdn space enter, or a
single character; "*N" repeats; `idle:N` is N passes of the main loop without a key
(docs/town_loop.md). Without `--screenshot` a window opens and play continues with the
keyboard.
