# The townspeople

Each town map has eight people (their names are in the .MTP file, 0x005). They walk from one
of the map's entrances to another, go in, wait, come out and walk on. They are figures 16-23 of
the program's figure tables. Rebuilt in `src/game/townspeople.{h,cpp}`, checked against the
original's memory by the reference rig (docs/reference.md).

## Their records

F:d390 + 0x1a i, 26 bytes each (only these are used in town):

| Offset | What |
|---|---|
| +0, +2 | where the person was placed (x, y) |
| +4, +6 | the target: the entrance it walks to |
| +8 | high nibble: the entrance it last went into; low nibble: the one it walks to |
| +9 | rounds left inside; 0 = out walking |

Its position is figure 16 + i's (F:4004/4036, so 4024/4056 for the first), (0, 0) while
inside; its heading (the way it last stepped) is F:3920 + 16 + i, its walking animation
the figures' (DGROUP:01f6, 396c, 409a; docs/town_loop.md).

## Placing them (1000:2da8)

When a map is loaded for a new game (DGROUP:014c clear), person i draws a random number r:
its target and position are entrance `shuffle[r & 7]` (the .MTP's 8 bytes at 0x215), its last
entrance r & 7, and it is inside for 1 round. Its frame is 0x10 and its script direction none.
On maps above 10 nobody is out.

So the first round brings each person out at entrance r & 7, which is usually not where it was
placed, and it then walks to `shuffle[r & 7]`.

The random numbers before that, in a new game: 256 at start-up (`1000:50c8` fills a table at
P:09fb), 8 in the new-game setup (`1000:4f41`: for each of figures 0-3 and 12-15,
sprite base 0x92 if the number is odd), then the 8 here, then one more before the main loop's
first pass that is not located yet. The image starts the generator at 04 03 02.

## Every third game tick (1000:24c2)

For each person in turn:

- Inside (+9 not 0): the count goes down; at 0 the person comes out at its last entrance
  (position = entrance[+8 >> 4]). (If F:d339 is set, the first person's count is set
  to 0xff first, so it stays in. It is clear in town; what sets it is not read yet.)
- Out: only if it is near the party does it move. Near means, comparing the words as plain
  numbers, x above (party x & 0x0f70) - 16 and below (party x | 0x0f) + 16, and the same for y
  with 0xf070; the sums step across area edges like a position (`1000:191b`). So people far
  from the party stand still.
- A step (`1e31:0006`, below), then the walking frame: if its heading is not the direction of
  its current script, the character script for that heading starts over; the next frame is
  taken either way, even without a step.
- At the target: it goes in for (random & 31) rounds, its position becomes (0, 0), and the next
  entrance is `shuffle[+8 & 7]`; +8 shifts left 4 and takes it as the low nibble.

**Original bug (kept):** a person sent in for 0 rounds never comes out: the count is only
counted down while it is not 0, and a person "out" at (0, 0) is never near the party. One
arrival in 32 loses a person for the rest of the map. `Townspeople::fix_lost_walker` brings it
out the next round instead.

## A step (1e31:0006)

`287f:0971` gives the direction from the person to the target, the one of eight nearest the
line between them (north 0, clockwise), from four tests on dx and dy (|dx| <= 2dy north,
|dx| <= -2dy south, |dy| <= 2dx east, |dy| <= -2dx west; P:0240 maps the four bits to a
direction, -1 at the target). At the target the person does not move.

Otherwise the person turns one eighth toward it (toward the nearer side; it keeps its heading
if already facing it), then tries eight directions in turn, each turned from the last by the
next entry of DGROUP:310a read backwards: straight on, one eighth right, one left, two right,
two left, three right, three left, behind. Every 30 steps (any person's, DGROUP:315a, 100 at a
new game) the pattern flips to start left first (DGROUP:315c).

A direction is taken if:

- the map cell under the new half-cell (the cell holding x + 1, as for the party's leader) is
  below the blocking tile (DGROUP:0150); and
  - the cell is read from the 24x24-cell window that `287f:1314` fills with the 3x3 blocks around
    the party's block (P:07ad), not from the map (`1e31:023c`). The index is the view's corner
    cell P:09ed (((party y half-cell >> 1) & 7) + 2) * 24 + ((party x half-cell >> 1) & 7) + 2,
    plus the new half-cell relative to the view (x - party x + 0x1a, y - party y + 0x0c, as
    `1000:2689` computes it across area edges): (rel y >> 1) * 24 + (rel x - 13) >> 1, one more
    when rel x is even and the party's x odd, 24 more when rel y and the party's y are both odd.
    Rows are 24 cells, so a person past the window's right or left edge (a person "near" the
    party can be up to about 25 half-cells to the side) is checked against a cell of the next or
    the previous row. Found on the original: a person going south-east by the training center
    with the party far west was stopped by such a cell and went south. An index before the
    window reads the memory there, the block values, the area grids and the masks, and the
    rebuild reads the same (docs/wilderness.md); past the window's end it reads the real cell and
    logs it (not met on any path tested); and
- no other figure stands exactly there (`1e31:16ab`): a character of the party where it was
  last drawn (the end of `1000:051b` records them), or another person. Then a 'Mech figure
  (0-3 and 12-15, active or not) blocks three half-cells of its row: its x, and one either side
  (stepped with `1000:191b`). In town that is the party's 'Mech during a training mission's walk. If the place taken is the target itself,
  F:d57e is set (read elsewhere, not rebuilt yet).

Then the person moves there and its heading becomes that direction. If none can be taken it
stays put, heading unchanged.

The party's places are those of the last drawing. After a key that drawing comes right after
the party's step (`1000:0081`), so a person never steps onto the half-cell the party has just
moved to.

## Drawing (1000:051b)

After the party, each person out walking (+9 is 0) is drawn if in view: within 13 half-cells
across and 12 down of the party (in the same area; further areas by a wider test). The sprite
is slot F:d55e[16 + i] (0xfe for all: the grey people of MECHSHAP.CMP's row 40) plus its
frame; it is drawn where the party's leader would be if it stood there. In water it sinks as
the party does, with the quarter bits of DGROUP:032e (9, 3, 12, 6 by the odd/even half-cells).

## Meeting them

Nothing happens. The party's step check (`1000:1c12`) does not look at the townspeople, so the
party walks over them; a person only steps around the party. Space opens the command menu
(`1000:2c50`, not rebuilt yet), which is not about them.

## Not rebuilt

- `1000:2a93`: the map's objects (up to 64, F:d457 sprite, d497 area, d4d7 x, d517 y;
  sprites 0x7c/0x7d swap each drawing). All empty in town at a new game.
- The 'Mechs' figures (0-3, 12-15): none in town at a new game.
- What a saved game restores (DGROUP:014c set skips the placing).
