# Saved games (GameN)

The command menu's items 5 and 6 load and save a game (docs/town_loop.md): `1000:32b3` and
`1000:35d3`. A saved game is one file in the game's folder, named `Game` and the slot's digit
(DG:0154, the digit at DG:0158 = '1' + the slot), slots 0-5 from menu 0x28, so `Game1` to
`Game6`. The menu's item 6 cancels.

## The file

3913 bytes, little-endian, no header beyond the first byte:

| Offset | Size | What |
|---|---|---|
| 0 | 1 | 0x0c, the format's mark; a load refuses any other |
| 1 | 0xf44 | F:c614-d557, as it is in memory |
| 0xf45 | 2 | the party's x (1c6c:a44b) |
| 0xf47 | 2 | the party's y (1c6c:a44d) |

The block from F:c614, by what is known of it (offsets in the block are address - 0xc614):

| F: | Size | What |
|---|---|---|
| c614 | 16 x 0x11 | the character records (docs/formats/characters.md) |
| c724 | 8 x 0x7d | the 'Mech records (docs/formats/mech.md) |
| d10c | 0x0c | not known |
| d118 | 8 x 0x10 | records whose byte 0 the cache and the new game write (docs/cache.md); not known |
| d198 | 0x174 | not known |
| d30c | 0x64 | the script flags (docs/scripts.md) |
| d370 | 4 | C-Bills |
| d374 | 3 x 4 | the three investments |
| d380 | 0x10 | not known |
| d390 | 8 x 0x1a | the townspeople's records (docs/townspeople.md) |
| d450, d451 | 1 + 1 | the care's kits (docs/combat.md) |
| d452 | 4 | byte 0 of 'Mech records 0-3 while held at the garage |
| d456 | 1 | the next character name number |
| d457 | 64 x 4 | the map marks: codes, then areas (d497), x (d4d7), y (d517) |
| d557 | 1 | the marks' next slot |

So a save holds the party, its 'Mechs, the flags (the story so far), the money, the
townspeople and the marks, and the place; not the random numbers, the menus' last items, the
combat settings, or anything the load rebuilds below.

## The save: 1000:35d3

1. With F:d34e set (the cache's second level) it refuses: the message box, DG:06d2, a key.
2. Window 3 cleared, DG:0673, and menu 0x28 (`1656:0b5e`): item 6 ends it.
3. The disk check: `1000:28cc(3)`, then INFOCOM.CMP (DG:06a1) opened and closed, the disk
   prompt (`1000:2913`) until it opens.
4. The file created (`287f:33d0`, modes 0x8101 and 0x180), the four writes above (`287f:366a`),
   and closed. A failed create or a short write shows DG:06ad in window 3 and waits for a key.
5. `0e31:032f(0)`, the panel (`1000:4cac(1)`), `1000:28cc(1)`.

## The load: 1000:32b3

1. Window 3 cleared, DG:05c7, menu 0x28; item 6 ends it. The disk check as above (DG:05f5).
2. The file opened; if it cannot be: DG:0640, the file's name, DG:0658, a key, and the end.
3. The first byte read; if it is not 0x0c: DG:0601, a key, and on to step 6 with nothing loaded.
4. Area 0xcc's map 1 and area 0x34's 0 (27e8:00fc, 0064); the block and the position read.
5. Then rebuilt from what was read:
   - the 33 bytes at F:45de (the cache doors' used items) 0;
   - the word at 02f8 of the segment named at DG:53e6 = flag 4f (F:d35b), sign-extended;
   - area 0xcc's map 11 when flag 04 (F:d310) is set, area 0x34's map 12 when flag 32
     (F:d33e) is set (docs/wilderness.md);
   - with flag 3a (F:d346, the cache) clear, the nine-map load around the position (`104e`, then
     `2da8` for each grid with a map, `1da8`); with it set, the cache's own load (`0b5d:0004`);
6. For the 'Mech figures 0-3: the sprite base 0x92, or 0 when the record's byte 0 is 'L' (the
   same test as an encounter's, docs/combat.md); the frame (F:409a), the heading (F:3920) and
   the direction (F:396c) 0; the far pointer at DG:01f6 + 4i = 27e8:0270. F:374a (the double
   agent found) 0. Outside the cache, with tile set 2 loaded (F:3988), `1000:4621` loads
   BTTLTECH.ICN back.
7. `0e31:032f(0)`, the panel (`1000:4cac(1)`), `1000:28cc(1)`.

The load does not touch the random numbers, so a game loaded goes on with whatever numbers the
session had.

## In the rebuild

Not rebuilt yet: the menu items log what they would do.
