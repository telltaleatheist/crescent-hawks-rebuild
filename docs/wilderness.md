# The land outside the towns

The world is 16 x 16 *areas*, each 64 x 64 cells: 8 x 8 *blocks* of 8 x 8 cells. A position's
x bits 8-11 and y bits 12-15 name its area (formats/mtp.md); in this document an area is the byte
`0xYX`, so the town of map 1 (the new game's start, x 0x0c45, y 0xc019) is area `0xcc`. A town's
map covers some of its area's blocks (map 1 covers all 64). Every other block of the world is a
*terrain block*, which the program makes up as the party comes near, from one height per area and
a table of random numbers. The same place always gets the same terrain.

The program keeps the nine areas around the party's, in the asm display library's data (P, 2c6c):

| P: | Size | What |
|---|---|---|
| 0324 | 9 x 64 | each block's *neighbour mask* (below), grid by grid |
| 0564 | 9 x 64 | the nine *grids*: one entry per block, row by row; grid k is the area 16 (k / 3 - 1) + (k % 3 - 1) from the party's, so grid 4 is the party's own |
| 07a4 | 9 | the window's nine blocks, mask OR entry (287f:13d9's result) |
| 07ad | 24 x 24 | the *cell window*: the 3 x 3 blocks around the party's block, one tile number per cell |
| 09fb | 256 | the noise table |
| 0b0b | 256 | the world's heights, one per area, row by row (`tables.world_heights`) |
| 0c1d | 64 x 64 | the block store: 16 terrain templates (`tables.terrain_templates`), then the loaded map's blocks (1000:2da8 reads its cells to 0x101d) |

A grid entry below 0x80 is a terrain type (0x00, 0x10, 0x20, 0x30 or 0x40); 0x80 and up name a
block in the store, 0x80 + n for block n, so the map's blocks are 0x90 onwards.

## The noise table

Start-up (1000:50c8) draws the first 256 random numbers (287f:0bc0, from the program image's state
04 03 02) into P:09fb, before anything else draws one (docs/startup.md). Nothing writes it again.
The rebuild computes it the same way (`Terrain::start_up_noise`); the rig compares it with the
original's memory in every case (`state noise`).

## The generator: 287f:104e

`287f:104e(area)` makes all nine grids, then their masks (287f:1886). For grid k it takes the
heights of four areas: its own (the north-west corner), the next east, the next south and the
next south-east, `world[a]`, `world[a + 1]`, `world[a + 16]`, `world[a + 17]` for
`a = area - 0x11 + 16 (k / 3) + k % 3`. So an area's corner heights are shared with its
neighbours: the land runs on across area edges.

The index is not wrapped. At the world's west and east edges the next column is the next row's
first or the last row's; above the first row the program reads the noise table's end, below the
last row 18 bytes of 0 and then the templates. (Bug 31 is the same at a map load's columns.)

**`287f:0bfb`** fills a 9 x 9 lattice of heights (P:02d3; the ninth row and column are the next
areas' first) by midpoint displacement. Every point starts empty (0xff) but the corners.

1. **The edges** (287f:0d07 across, 287f:0d79 down): top, bottom, left, right, in that order.
   An edge is halved again and again: the midpoint of an empty point between two made ones is
   their average plus a *displacement*. The halves wait on a stack and the second half is done
   first. Each edge's noise starts at entry *sum of its two corners* of the noise table and moves
   one entry for each point made: two areas meeting along an edge make the same points on it.
2. **The inside** (287f:0df8) goes on from where the right edge's noise ended. Each square gets
   its centre, the average of its four corners with no displacement; then the midpoints of its
   top, left, bottom and right sides, if still empty, each its two ends' average displaced; then
   its four quarters, the last (south-east) quarter first.

The displacement over a half of *c* lattice cells: the next noise byte AND (4c - 1), less 2c,
added in 8 bits; a result of 0x80 or more (past the top, or below zero) becomes 0. So a half of
4 cells moves by -8..7, of 2 by -4..3, of 1 by -2..1.

**Bug 37** (`fix_terrain_noise`, off): the inside's right sides use c / 8 (that is, 0) for their
displacement. The mask is then 0xff and nothing is taken off: the whole noise byte is added, and
the point usually comes out 0 or high. The left sides divide the down distance by 8, which is in
ninths of the lattice (9 per row) and gives the right c; the right side divides the across
distance, already in cells. The fix uses c. It changes all the terrain, so the rig runs with it
off.

The area's types are its lattice's first 8 x 8 points AND 0xf0. The world's heights are 0-0x50;
the shipped table gives types 0x00-0x30 in the areas around the towns.

## The map's blocks: 1000:2da8

The map load writes the map's blocks into its grid (2da8's first argument; the new game uses grid
4): `0x90 + n` for the map's block n, at the block column and row its header gives, `width / 8`
by `height / 8` of them. The rest of the grid keeps its terrain. It then rebuilds the masks
(`287f:1da8`, the far call of 1886). The new game (1000:4dc7) runs `104e(0xcc)`,
`2da8(4, 1)` and `1da8`.

Each area's map comes from a 256-entry table (at the segment in DG:53d0, offset 0x30 + area;
flags change two entries first, 1000:3419-3435: area 0xcc map 11, area 0x34 map 12). A
load into all nine areas (1000:3469, 1000:3edc) loads each grid's map in turn. None of the areas
around 0xcc has a map. Not rebuilt yet: the rebuild loads map 1 into grid 4 only, as the new game
does.

## The masks: 287f:1886 and 287f:11bb

`287f:11bb` gives each block of a grid a 4-bit mask of the neighbours with the same entry: north
1, east 2, south 4, west 8. A block's neighbour across the grid's edge is in the next grid. A map
block's mask is instead its number in the store (entry - 0x80), except that entry 0x80 on a
grid's top or bottom row (not a corner) is compared like a terrain type.

The grids sit one after another in memory, and 11bb finds a neighbour across an edge by a fixed
offset: west of column 0 is 0x39 bytes back (column 7 of the grid before), north of row 0 is
0x88 bytes back (row 7 of the grid three before). For the edges of the 3 x 3 grids as a whole
these land in other memory (the masks, the cell window). The window only uses the blocks next to
grid 4 (below), whose neighbours are all real; the rebuild compares the other edges' neighbours as
different.

## The window: 287f:1314 and 287f:13d9

`287f:1314(x, y)` fills the cell window with the 3 x 3 blocks around the block holding the
position (its block column `(x & 0xff) >> 4`, row `(y & 0xff) >> 4`), one `287f:13d9` call each,
and keeps each call's result (mask OR entry) at P:07a4. A block one past grid 4's edge is taken
from the next grid. The map drawer (287f:18ef) and the figures read the window, not the map, so
the cells past a map's edge are terrain.

13d9 copies one block into the window:

- **A map block** (entry 0x80 or more): its 64 bytes from the store, as they are.
- **Type 0x10**: `tables.plain_block`, whatever the mask.
- **Types 0x00, 0x20, 0x30, 0x40 and up**: the template for its mask, each template byte
  below 0x40 turned into `(byte & 0x0f) + offset`, the offset 0x00, 0x10, 0x20, 0x30 for those
  types; bytes of 0x40 or more stay as they are. The template is turned by the block's place,
  `(row + column) & 3` in the area: 0 as it is, 1 upside down, 2 mirrored, 3 both. A turned
  block uses the template of the turned mask, and its bytes below 0x40 are turned too, through
  `tables.mirror_north_south` (upside down) and `tables.mirror_east_west` (mirrored). It is
  built in P:20dd and copied from there.

13d9 also works out a second value from the block's place, `((row + column) << 2) & 0x10`, and
loads it, but never uses it.

The step check (1000:1c12) reads P:07a4: a block of type 0 whose four neighbours are all type 0
(0x0f) is deep water. 1000:4d57 and 203b:000a read its bit 0x80, a map block (combat.md 2.15).

## Not rebuilt yet

- The steps across an area's edge outdoors. The position steps (287f:158c up, 287f:17c5 right and
  the others; `world.h` rebuilds their position part) also move the grids over by a row or a
  column there, make the new row or column (287f:18d8 and 0bfb for its three grids) and rebuild
  the masks.
- The area-to-map table and the nine-map load (1000:3469, 1000:3edc), 1000:42c8, and
  24d3:12db's `104e(0x89)`.
