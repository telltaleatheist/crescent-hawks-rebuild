# Combat

How a fight starts, how it runs and what it leaves behind, as read from BTECH.EXE. Nothing here
is rebuilt yet. The records it works on are in [formats/mech.md](formats/mech.md) (the 'Mech
record and the tables in the EXE) and [formats/characters.md](formats/characters.md) (the
character record and the per-figure arrays).

Each statement is from the code unless it says "guess". "Checked" means it was read
instruction by instruction more than once; "open" items are collected at the end.

## Conventions

**Addresses.** Code is `ssss:oooo` (load segment 0x1000 included; Ghidra names the function
`FUN_ssss_oooo`). Data addresses carry the prefix of the segment the code really uses: `DG:`
(DGROUP: texts, rule tables, settings), `F:` (far data: flags, C-Bills, the character and 'Mech
records, the figure arrays), `P:` (the party position, the map cells), `M:` (the menu state) and
`27e8:` (the 'Mech templates). docs/original.md, "Data segments", defines them.

**Positions** are two words (docs/movement.md): the low 7 bits of each count half-cells in a
64x64-cell area, x bits 8-11 are the area column, y bits 12-15 the area row. Combat measures in
half-cells on a "linear" grid: X = column x 128 + (x & 0x7f), Y = row x 128 + (y & 0x7f).
Steps that cross an area edge are fixed up by carrying into or borrowing from the area bits.

**The party position as a cursor.** Most combat helpers measure from the party position, not
from an argument. The code first moves the party position onto the figure that acts (with
`1000:17bb`, which walks it there a half-cell at a time and loads map areas, or `1000:186f`,
which only sets it) and puts it back afterwards. The rebuild passes the position as an argument.

**Figures.** A fight has 24 figure slots: 0-3 the party's 'Mechs ('Mech records 0-3), 4-11 the
party's eight characters on foot (character p is figure 4 + p), 12-15 the enemy 'Mechs ('Mech
records 4-7, figure - 8), 16-23 the enemy people (character records 8-15, figure - 8). Outside a
fight figures 16-23 are the townspeople. Details in formats/characters.md.

**Random numbers.** Three primitives, all from the one generator (docs/town_loop.md):

| name | code | draws |
|---|---|---|
| r | `287f:0bc0` | one raw byte 0-255 |
| d6 | `1000:19f3` | r & 7, drawn again while it is above 5, plus 1: one or more raw draws (one with chance 3/4) |
| 2d6 | `1000:19dd` | two d6 |

Every draw counts: the rebuild must make the same draws in the same order, including draws whose
result is thrown away.

## 1. How a fight is reached

`203b:000a(kind)` (FUN_203b_000a) runs every fight. Its one argument picks the kind:

| kind | caller | what |
|---|---|---|
| 0 | the town's game tick, `1000:02ae` | a random encounter outdoors |
| 1 | the mission driver `17dc:0629`, modes 2-7 | a training mission or the raid |
| 2 | the mission driver, mode 8 | the Arena |
| 3 | the mission driver, mode 9 | the fights of mode 9 (Jason alone on foot) |

Kind 0 is the only one that builds its own enemies, asks "Engage in combat?", allows evading,
and gets the salvage tail; kind 3 shares that tail; kinds 1 and 2 get the mission tail. For
kinds 1-3 the mission driver calls `17dc:0d49` to set up the opponents first.

### 1.1 The random encounter

In the game tick (docs/town_loop.md, step 2) one r is drawn every tick. A fight of kind 0
starts when the party is outdoors (flag 04, F:d310), (r AND flag 24 (F:d330)) is 0, and
F:d346 (flag 3a) is 0. Flag 24 is a mask: with 0 every tick qualifies, with 0x7f one tick in
128. The double-agent ending sets flag 24 to 0x7f (2.9). Where else flag 24 is set is not
traced. A tick that qualifies does not always give a fight: the encounter set-up may place
nobody, and the fight then ends at once (2.3).

### 1.2 The training center: specials 1-3

TRAINING.BLD uses specials 1, 2 and 3 (docs/scripts.md, "Specials").

- **Special 1** (`24d3:0027`): chooses Jason's 'Mech by flag 02: 0 the LOCUST template
  (27e8:02f0, sprite base 0), 1 the WASP (27e8:036d, sprite base 0x92), 2 the CHAMELEON
  (27e8:04e4). It sets Jason's 'Mech slot (character 0 byte 0x0c, F:c620) to 0, copies the
  0x7d-byte template into 'Mech record 0 (F:c724) and sets its byte 0x79 (pilot) to 0. Unless
  the demo plays, it shows picture 0. The party is put at (0x0c3c, 0xc04f), figure 0's walking
  script is set (DG:01f6 = 27e8:0280) and the 'Mech walks in four half-cells east, one walking
  frame and six frames of map animation per step. Then it runs the mission driver with
  flag 00 (the mission number).
- **Special 2** (`24d3:0187`): prints the 'Mech's name by its first letter (DG:48b3 for 'L',
  DG:48df + DG:48ee for 'W', DG:48e4 + DG:48ee otherwise).
- **Special 3** (`24d3:01c2`): Jason leaves the 'Mech: 'Mech 0 byte 0 = ff, character 0's
  'Mech slot = 8 (on foot), the party at (0x0c2e, 0xc076). In town it also removes every map mark
  whose code is 0x7c or 0x7d (with or without bit 7) from the 64-entry mark ring (3.13): the
  fires left on the training field, a guess.

### 1.3 The mission driver `17dc:0629(mode)`

FUN_17dc_0629. A jump table at cs:086f by mode. All modes share a walking loop like the town's
(a key is a step, `1000:218f`; with no key the tiles animate and the townspeople move) and the
results step at 0bf2. The callers: special 1 (modes 0-7 from flag 00), ARENA.BLD's special 0x23
(`24d3:10e2`, mode 8) and `19b8:152f` (mode 9, run from JAIL.BLD's special 0x28).

| mode | what happens | pass (flag 01 = 1) |
|---|---|---|
| 0 | A walking course. P:200a is saved and cleared. Each step counts 1, plus (F:32ae of figure 0) >> 3 when that is set (heavier terrain counts more, a guess). The course ends when the party reaches x >= 0x0c78 and y >= 0xc07c (text DG:14a1, a key). | reached, and the count (less 50 if the 'Mech is a Locust) below 215. P:200a is restored. |
| 1 | A marked spot. One r & 7 picks a spot from DG:1632 / DG:163a; that map cell in P:101d is saved and set to tile 0x50. With a Locust a message (DG:1434) comes first. The walk ends on the cell just below the spot (text DG:14ef with a Locust, else DG:152a, and the cell is put back). | the 'Mech is not a Locust (it has no jump jets: a guess). Either way Piloting (character 0 byte 8) + 1 if below 3. |
| 2 | Target practice. One enemy 'Mech (`0d49(1, 0)`), then its record 4 is disarmed: both actuator bytes 0x24 / 0x25, all ten ammunition bytes 0x27-0x30 and both MP bytes 0x31 / 0x32 set to 0. It cannot move, kick or fire. Fight kind 1. | as below |
| 3 | 3-6 enemy 'Mechs (`0d49(r & 3 + 3, 0)`), fight kind 1. See bug 29. | as below |
| 4-7 | 1-4 enemy 'Mechs (`0d49(mode - 3, 0)`), fight kind 1, with F:0090 = 1 (no "computer fights for you" question, no Computer order). Mode 6: one r, the raid if r & 1. Mode 7: always the raid. In the raid F:3772 = 1 and the 16 cells P:101d + 0ff0..0fff are set to tile 0x40 (a gap in a wall: a guess). F:0090 = 0 afterwards. | as below |
| 8 | The Arena: `0d49(0, 0x81)` (no enemy 'Mechs from the loop, one enemy person, figure 4 active), fight kind 2. | not set by this mode |
| 9 | Jason alone on foot (`19b8:152f` first hides characters 1-7 and the four 'Mechs, saving their first bytes at F:3fe9 + p and F:d452 + m, and puts the party at (0x0d10, 0x7024)). F:398e = 1. Every key is a step; on every 80th step `0d49(0, 0x88)` (eight enemy people) and fight kind 3. The mode ends when DG:014a is 0 after a fight. On y = 0x702d, at x = 0x0d14 + 4i (i < 4), texts DG:155d, 159a, 15d4 the first time and a call to `19b8:16b2`. | not set by this mode |

The results step (all modes): flag 01 (F:d30d) = 0 first.
- Modes 2-7 pass when Jason's 'Mech still exists (F:c724 != ff) and F:3992 is 0 (the party did
  not flee; see bug 13).
- Modes 5-7: Gunnery (character 0 byte 7) and then Piloting (byte 8) each go up by r & 1. The r
  is drawn only for a skill below 4.
- After the raid (F:3772): flag 04 (outdoors) = 1, flag 05 = 1 if Jason's 'Mech exists. F:3772 = 0.
- F:398e = 0. If F:4594 is set, the place's script is deciphered again (`17dc:1d30`): the fight
  reused the script buffer.

After a fight in modes 2-8, `17dc:134b` puts the party position on figure 0, or on figure 4 when
Jason is on foot (F:c620 = 8), and redraws.

### 1.4 Opponent set-up `17dc:0d49(a, b)`

FUN_17dc_0d49. `a` is the number of enemy 'Mechs, `b` the number of enemy people (each with bit
0x80 as a flag).

1. Saves the townspeople's positions (figures 16-23) into their records, F:d390 + 0x1a i, at +0
   and +2 (docs/townspeople.md). The fight puts them back (2.13).
2. Empties 'Mech records 4-7 and character records 8-15 (byte 0 = ff). Every figure: destroyed
   flag 0, active 0, x = y = ffff.
3. b below 0x80: figure 0 active (Jason's 'Mech); else figure 4 (Jason on foot).
4. The place. If a has bit 0x80 (an outdoor site, a guess): x = 0x0a10 + (r & 3), then an r, and
   if r & 1, x = 0x0a28 + (r & 3); y = 0x806f + (r & 3). The 'Mech type comes from the
   encounter table DG:13e2 at index 2d6 - 2 (or (r & 3) + 3 when F:e48e is set), with the
   sprite base DG:140e at the same index (formats/mech.md). With F:e48e set, a = 2 if any of
   F:d452..d455 is ff. **None of the five calls of `0d49` passes bit 0x80 in a**, so this
   branch is never reached in DOS 1.03 (open question 1).
   Otherwise: x = 0x0c65 + (r & 7), y = 0xc059 + (r & 7) (the training field); b with bit 0x80:
   (0x0d10, 0x7024); flag 00 = 2: x = 0x0c72. The template is LOCUST (27e8:02f0).
5. a = a & 0x7f; in the raid (F:3772) a = 4. For each i below a:
   - character 8 + i: byte 0 = 0, its 'Mech slot (byte 0x0c) = 4 + i;
   - 'Mech record 4 + i: the template's 0x7d bytes. In the raid the template is JENNER
     (27e8:0561). With F:e48e and i > 0, record 5 is also filled from 27e8:05de (the
     "spectator", 3.14);
   - flag 03 (F:d30f): 0 in the raid; clamped to at most 6. Each armour byte 0x11-0x1b is cut by
     DG:165a[flag 03], to 0 if it is not more than the cut;
   - flag 03 above 4: five draws, each picks slot byte 0x34 + (r & 0x1f) and sets its destroyed
     bit if the slot is not empty;
   - flag 03 above 5: engine hits (0x75), sensor hits (0x77), gyro hits (0x76) = r & 1 each, in
     that order;
   - so a higher flag 03 makes the enemy 'Mechs weaker;
   - figure 12 + i: sprite base, frame 0x0c, walking script 27e8:02a0 (DG:0226 + 4i), heading
     and facing 6, active, position = place + (DG:1642[i], DG:164e[i]).
6. People: if b is not 0, b = (b & 0x7f) + the next character number, at most 16. For each
   remaining character j up to b: byte 0 = 0, 'Mech slot 8 (on foot), sprite base fe, frame
   0x1c, walking script 27e8:02e0, heading and facing 6, active, position from the same offset
   tables at index j; skills 4-9 = r & 1 each (6 draws, Medical is not set), byte 1 = 2d6,
   byte 0x0f (hit points) = 10 x byte 1, byte 0x0b (weapon) = r mod 14.

### 1.5 The Arena and the raid

- **The Arena.** ARENA.BLD's special 0x21 calls `17dc:1c9b`: 'Mech record 0 is saved at F:3780
  and replaced by the LOCUST template, characters 1-7 are hidden (first bytes saved at F:3fe9),
  all eight characters' 'Mech slot bytes are set to 3, and F:e48e = 1. Special 0x23 runs mode 8
  and then `17dc:1b41`, which undoes it (slots back to 8, F:e48e = 0). F:e48e marks figure 13 as
  the "spectator" (3.14). How the Arena's enemy 'Mechs are made is open (open question 1).
- **The Kurita raid** (F:3772, modes 6 and 7): four JENNERs, the armour cut off, a story message
  first (2.4), one more story line per round (2.11), and fleeing only by walking out of the
  area (2.7).

## 2. The fight routine `203b:000a`

FUN_203b_000a, 5240 bytes. No return value.

### 2.1 Set-up

1. DG:2b20 = 12 (the weapon target picker's first candidate, `1d43:07cb`).
2. The party position at entry is saved.
3. The nine bytes P:07a4..07ac are copied (only their bit 0x80 is used, in 2.15). They sit just
   before the map cell buffer P:07ad; one per loaded map block is a guess.
4. Kind 0: the townspeople's current positions (figures 16-23) are saved into their records
   (+0 / +2), as `0d49` does for the other kinds.
5. The per-figure tables are cleared for all 24 figures: paths F:40b4 to 2 (the end mark), targets
   F:3800 to ff, orders F:32c6 to ff. Kind 0 also clears every figure (destroyed 0, active 0,
   terrain F:32ae 0, heading ff, x = y = ffff); for kinds 1-3 `0d49` did that.
6. For the eight 'Mech records: heat F:006e, Inferno count F:d576 = 0; the computer-control
   flags F:3994 for figures 0-11 = 0.

### 2.2 Placement

The party is placed from the saved position each time.
- Characters on foot first (character exists and byte 0x0c >= 8). The k-th one placed goes at
  the party position plus formation offset k (DG:3a16 / DG:3a1e, eight places: the first at the
  party's own half-cell, the next ones around it). Its active word is incremented, not set.
- Then the party's 'Mechs that exist, the k-th at offset k of DG:3a26 / DG:3a2a (four places,
  the first two half-cells north).
- k counts only members that exist, so the first 'Mech always gets the first place whatever
  its slot.

### 2.3 The encounter (kind 0 only)

1. `15ab:0d3d` (FUN_15ab_0d3d) builds the encounter:
   - The view is set on the party. Offsets: dx = (r & 7) + 10, negated if r & 1; dy the same (4
     draws in the order dx, sign, dy, sign). The search starts at that view cell, 10-17
     half-cells away on each axis.
   - Figures 12-23 are cleared.
   - Enemy people, characters 8-15 in order: byte 0 = ff, then one r; if r & 1 a soldier is
     made: name number 1, on foot; weapon = DG:2cf4[sum of 7 draws of r & 3]; byte 1 = 2d6,
     hit points = 10 x byte 1 (8-bit); byte 2 = 2d6; the seven skills = r & 3 each; armour type
     = r & 3; armour points = 2d6 + 2d6, whatever the type.
   - Enemy 'Mechs, records 4-7: byte 0 = ff, then one r; if r & 1 and the party's 'Mech m - 4
     exists, one more r picks the template DG:2df8[r mod 3] (LOCUST, WASP, STINGER) and the
     sprite base (0 for the Locust, else 0x92). So there are never more enemy 'Mechs than party
     'Mechs. The armour cut and the destroyed slots of `0d49` are not applied.
   - Placing: a cursor steps right from the start cell, wrapping to the next row after 16 steps.
     A person needs a cell whose tile is above 0x0f and below DG:0150 (the blocking tile); a
     'Mech needs that cell and its neighbour. Someone whose cell is outside the view is dropped
     (record emptied). After each 'Mech the cursor moves 3 on. No draws.
2. If any enemy figure is active, `203b:28db` asks whether Jason can reach the first one: from
   figure 4 (or Jason's 'Mech), up to 30 half-cell steps of the greedy stepper `1e31:0006`,
   ignoring other figures and terrain cost. No: no fight.
3. With no enemy present or none reachable: no fight; the view goes back (2.12).
4. "Attacking force:" (DG:3402), the number of enemy 'Mechs with " 'Mech" / "s" if more than
   one and " and", then always the number of enemy people placed with " human" and "s" unless
   it is 1 (so "0 humans").
5. "Engage in combat?" (DG:3428), yes/no, default yes.
6. Declined: one r; r & 3 not 0 (3 in 4) evades (no fight, 2.12). Otherwise "You did not evade
   them!" (DG:343a), a key, and the fight goes on.

Kinds 1-3 always fight.

### 2.4 The options before the first round

1. In the raid: a message box (DG:3452), a key.
2. Kind 2: the view is set on the party and drawn.
3. F:009e (the computer fights for the party) = 0. Unless F:0090 is set (modes 4-7), the
   question "Do you want the computer to fight for you?" (`15ab:0b5e`, DG:1115), yes/no with the
   old answer as default.
4. "Combat messages:" None / Brief / Verbose (`203b:24f0`, DG:3d3a) into DG:2e38 (0, 1, 2),
   remembered between fights.
5. "See combat graphics:" yes/no (`203b:2556`, DG:3d5e) into DG:2e3a, remembered. **This
   setting changes how many random numbers a fight draws** (section 5).

### 2.5 The round loop and the order of turns

Each round, in order:

1. The round-start party position is saved. F:3992 = 0.
2. **The party side.**
   - Player control (F:009e = 0): figures 0-11 with x or y = ffff are made inactive, and every
     party figure's path is reset. The orders menu `203b:14c3` runs (2.6); it returns 1 for
     Flee. Fleeing is handled (2.7). If nobody fled, each active party figure in slot order 0-11
     acts: one with F:3994 set is planned by the computer (`1e31:03ab(s, 0)`, section 4); one
     with orders has its movement points worked out (`203b:22bc` for a 'Mech with the type of
     its first order, `203b:2474` on foot) and its path simulated (`203b:193b`).
   - Computer control: `203b:1482(0)` plans figures 0-11 with `1e31:03ab`. If that set F:3992
     (a party person faced a 'Mech, 4.4) and the party has no 'Mech left: one r, and r & 3 not
     0 ends the fight as a flight.
3. If nobody fled:
   - F:374c = 0. `203b:1482(12)` plans every active enemy figure 12-23.
   - `22e8:000c(kind > 0)` runs the round (section 3).
   - The double agent (2.9), the exit zone, the end tests (2.8), enemy people running (2.10),
     the raid story (2.11).
4. With the computer in control: if F:3938 is 0 (no demo) and a key is waiting, F:009e = 0 and
   the key is thrown away. The player takes over next round.

So the turn order is: party orders, party plans in slot order, enemy plans, then the round,
where everyone moves and fires in slot order (section 3).

### 2.6 The player's orders

`203b:14c3` (FUN_203b_14c3) shows the first active party figure and a menu (window 3). The bar
starts on Begin Fight the first time.

| 'Mech (figures 0-3) | person (4-11) | what |
|---|---|---|
| Walk, Run, Jump | Move | `203b:1c1f`: add one waypoint with the map cursor |
| Use Weapons | Use Weapon | `1d43:0004`: assign targets per weapon |
| Kick | | `203b:2231`: needs both hip actuators; target picker with the Kick in slot 11 |
| | Clear Moves | orders deleted, computer control off |
| Computer | Computer | `1e31:03ab(fig, 1)`: hand the unit to the computer (not when F:0090) |
| Scan Unit | Scan Unit | `203b:2591`: Friends / Enemies / Cancel, then the unit list `15ab:1467` |
| Next Unit | Next Unit | the next active party figure, wrapping 11 to 0 |
| Flee | Flee | leaves the menu, result 1 |
| Begin Fight | Begin Fight | leaves the menu, result 0 |

- **A move order** (`203b:1c1f`): if the 'Mech's orders are of another type (walk, run, jump),
  the player is asked whether to change; yes deletes them. With no movement points left a reason
  is shown (no jump jets DG:3bad; overheated DG:3bca; leg damage or heat DG:3bf3; "You can't ...
  any further this turn" DG:3c33, with a hint to try running). Otherwise "You have N" movement
  points and the cursor: the eight direction keys move it (kept inside columns 0x0d-0x27, rows
  0-0x18); each move puts a tentative waypoint at the cursor and redraws the path; Space or Enter
  accepts. There is no cancel key. Each call adds at most one waypoint (up to 12 per figure).
  The number shown is not updated as the cursor moves. The target may be out of reach: the path
  stops where the points run out.
- **Use Weapons** (`1d43:0004`): a 'Mech that is shut down (heat 30) cannot (DG:2904); sensors
  damaged once is a warning (DG:292d), twice and it cannot fire (DG:2964). The list shows each
  weapon (up to 12), its ammunition (Full / Out / a count), its target and range band. A
  destroyed weapon's target is cleared. Listing a weapon clears its "held" bit. Choosing one
  that already has a target asks to override. The target picker (`1d43:07cb`) offers only enemy
  figures 12-23, with no range or line-of-sight check (the round decides).
- **Scan** (`203b:2591`): a person cannot see enemy 'Mechs while any is active; a 'Mech whose
  sensors are destroyed (byte 0x77 = 2) cannot scan 'Mechs.

The path shown for a figure (`203b:1774`) is drawn from the simulated steps as arrow glyphs.
Enemy paths are never drawn.

### 2.7 Fleeing

- Flee chosen: one r; r & 3 not 0 (3 in 4) escapes. The draw is made even where fleeing is then
  refused.
- Kinds 2 and 3: never (the result is set to 0).
- The raid: choosing Flee gives "You cannot escape these Jenners!" (DG:3497). Instead, if figure
  0 is outside the box x 0x0b7f-0x0cff, y 0xb07f-0xcfff at the orders step, the party escapes
  (value 2) without a draw.
- Failed: "You do not escape!" (DG:34b8) and the round goes on.
- Escaped in kinds 1-3: F:3992 = 1 (the mission is failed).
- Computer control with no party 'Mech left: r & 3 (2.5).

### 2.8 What ends a fight

Tested after each round, in this order (a later test can also end it):

| test | ends when |
|---|---|
| win | no enemy figure 12-23 is active (positions ffff are made inactive first) |
| enemy people run (kind 0) | 2.10 |
| mission (kind 1) | figure 0 (Jason's 'Mech) is not active |
| Arena lost (kind 2) | figure 0 not active, the exit zone not reached and flag 23 clear |
| exit zone | flag 23 (F:d32f) set and figure 0 active with x between 0x0900 and 0x0a07 (exclusive) |
| the leader | DG:014a is 0: set by `22e8:12c7` when character 0 or 1 is killed |

A flight ends it at once (2.7), before the round runs.

### 2.9 The double agent

After the round, if DG:014a, flag 27 (F:d333, a traitor exists) and F:374a (set by `15ab:18e8`
when the player has looked at the traitor: "looks very suspicious") are all set, the traitor
(character number in flag 25, F:d331) acts. Texts DG:34e9 to DG:387a, chosen by where he is:
- on foot: he is dealt with; a mark 0x7e at his place, his figure removed;
- piloting 'Mech m: with nobody else inside, or a second occupant whose Piloting is 0, the 'Mech
  is lost (record emptied, a wreck mark); if a second occupant is there he ejects (on foot,
  placed at the 'Mech). With a second occupant who has Piloting, the companion throws him out
  and takes the 'Mech;
- a passenger in 'Mech m: thrown out by the pilot.
Then his record is emptied, his figure inactive, flag 27 = 0, flag 24 = 0x7f.

### 2.10 Enemy people run (kind 0)

If F:374c was set this round (an enemy person was planned against a 'Mech, 4.4):
- With an enemy 'Mech still active: each active enemy person further than 25 (distance 3.6) from
  its first target is removed. "Some of the enemy humans have fled!" (DG:38bc) if any. No draw.
- With no enemy 'Mech: one r; r & 1 ends the fight with "The enemy humans have fled!" (DG:38e0).

### 2.11 The raid story

In the raid, one story line per round (five lines, far pointer table DG:3a2e), all remaining
lines at once when the fight ends. Before the second line, place picture O2 is shown
(`1000:48b7(2, 1)`, one draw). After the third, map 4 is loaded into block slot 0x0b
(`1000:2da8`) and 27e8:00fc = 0x0b.

### 2.12 After the loop

- **No fight** (kind 0 with no enemy or none reachable): the view goes back to the entry
  position (`203b:2aa3`).
- **Kinds 0 and 3**, fought, not fled and the leader alive: window 1; "After an invigorating
  scuffle, you defeated the Kuritans." (DG:38fc) if any enemy figure is destroyed; then the
  after-fight steps (2.15), flag 29 = 0, and healing `1c31:000a(0)` if any party character has
  lost hit points (the sum of 10 x byte 1 - byte 0x0f is not 0; not read).
  Declined and evaded, or fled: "You have eluded your enemies!" (DG:3936).
  Then, leader alive: the panel, "You return to where you were before combat took place."
  (DG:3954), and the party position is walked back to the entry position, the areas around it
  reloaded (`203b:2835`) and the view drawn.
- **Kind 1** (leader alive, not the raid): "You should return to base now." (DG:398b).
- **Kind 2** (leader alive, not the raid, the exit zone not reached): flag 21 (F:d32d) = 1 and
  "Congratulations! You have won!" (DG:39aa) if Jason's 'Mech exists; otherwise flag 21 = 0 and
  the crowd boos (DG:39ca).
- **The raid** escaped by leaving the area: "You have eluded your enemies!" (DG:39f7).

### 2.13 Always last

For every kind, the townspeople's positions are put back from their records; F:397c + i and
F:396c + i (i 0-7) = ff; the frame of figures 16-23 = 0x10.

### 2.14 What the fight writes back

| state | change |
|---|---|
| F:3992 | 1 when the party fled in kinds 1-3 or the computer planned a party person against a 'Mech; read by the mission driver |
| F:009e, DG:2e38, DG:2e3a | the auto-fight answer, the message level, the graphics setting |
| flags | 21 (Arena won), 23 (Arena exit opened, 3.14), 24 = 0x7f and 27 = 0 (traitor), 29 = 0 |
| character records | killed: byte 0 = ff; ejected: byte 0x0c = 8; hit points and armour; skills (3.5); the traitor removed |
| 'Mech records | armour, structure, items, actuators, hits, ammunition; byte 0 = ff when destroyed; the first letter kept in F:323e + m; pilot bytes 0x79 / 0x7a |
| F:d370 | C-Bills from scrap and pockets |
| the mark ring | wrecks, bodies, fires (3.13) |
| P:a44b / a44d | back to the entry position (kinds 0 and 3) |
| F:d390 records | the townspeople's saved positions (+0 / +2) |

### 2.15 The after-fight steps

**Repairs and scrap** (`15ab:0002`, FUN_15ab_0002). Run when the party has a 'Mech, an enemy
'Mech was destroyed and some character has Tech above 0.
1. The best tech: the highest Tech (byte 9) among the party, ties to the lowest index; S is the
   skill, I the character number. Text DG:0efb + name + DG:0efe, plus one of DG:0f9a's phrases for
   S 2-4, + DG:0f28, a key.
2. Armour: the pool is the sum of the wrecks' armour bytes (enemy figures 12-15 destroyed). Each
   location from the rear torsos (0x1b) down to the left arm (0x11), each party 'Mech in slot
   order, gets what it lacks (maximum at +0x45) as far as the pool lasts.
3. S >= 4: the same for internal structure, from the wrecks' structure.
4. S >= 2: every destroyed heat sink (item 0xa2) in a location that still has structure is
   mended (bug 22).
5. S >= 3: destroyed weapons are mended, one for each intact weapon of the same type in the
   wrecks, if the location has structure (bug 23).
6. Scrap money, only if N (wrecks) > 0 and I > 0: M = the sum of N draws of (r & 0x7f) + 90,
   times I (bug 24). Text DG:0f40, the amount, DG:0f65 ("s" if N > 1), a key; C-Bills + M.

**Salvage** (`15ab:04f9`, FUN_15ab_04f9). Run when a 'Mech figure was destroyed on either side, a
party character with Piloting above 0 is on foot, a party 'Mech slot is free, and none of the
nine bytes saved in 2.1 has bit 0x80. For each such character, the question DG:0fae (yes,
default). Yes: the best tech inspects (wait 120 frames); a wreck is picked: slot = (r & 3) +
12 x (r & 1), then the first slot from there (wrapping 15 to 0) whose destroyed flag is set; the
flag is cleared. It can be salvaged unless (engine hits 3, or gyro hits 2, or head structure 0)
and S < 4. Salvaged: the first free party slot gets the record (first letter from F:323e),
pilot = the character, engine and gyro hits = 1, centre torso and head structure at least 1,
sprite base 0 for an 'L', else 0x92 (bug 25). Not salvageable: DG:102f, and a new pick. No wrecks
left: DG:104c and it stops. See bug 26.

**Loot** (`15ab:094b`, FUN_15ab_094b), always on this path. If any figure 4-11 or 16-23 is
marked destroyed: "You dig through the soldiers' pockets and find" (DG:1085) a sum of (r & 0xf)
+ 3 per beaten enemy person (at least 2 in all) C-Bills. If weapons were found (bug 27), each
beaten enemy person's weapon is offered: when Jason is alone, as a swap for his own (DG:10e5,
default no); otherwise through `17dc:15e6`.

## 3. One round: `22e8:000c`

FUN_22e8_000c, 4795 bytes. The argument is 1 for kinds 1-3. It reads no keys except the message
pause.

### 3.1 Shape

A round is **12 phases**. In each phase, unless DG:014a is 0 (the fight is over):
1. **Movement**: every active figure, in slot order 0-23, takes at most one step of its path.
2. If anyone moved (always in phase 0) and graphics are on, the view is centred on Jason's figure
   and redrawn.
3. **Firing**: each active figure in slot order 0-23 tries its weapon slots 0-11 in order; the
   first one that fires ends that figure's turn for this phase. Each weapon fires at most once a
   round. DG:014a is checked before each figure.
4. **Waypoints**: for party figures 0-11 only, if the first order's place equals the figure's
   position, the order list moves up by one (the next leg's type now applies). Enemy orders are
   never moved up.

At the end, the party position is put back and `1e31:0c63` runs (3.15).

So in each phase the party fires first (party 'Mechs, then party people), then the enemy, and a
kill takes effect at once: later shots at a dead figure are cancelled.

### 3.2 Movement points

**A 'Mech** (`203b:22bc(m, type)`, FUN_203b_22bc; type 0 walk, 1 run, 2 jump, -1 none = walk):
1. MP = walking MP (byte 0x31). Jump: MP = jumping MP (byte 0x32) and go to step 5.
2. Hips: bit 3 of the two leg nibbles (bytes 0x24 and 0x25). Neither: MP = 1, skip step 3. One:
   MP = half the walking MP rounded up.
3. Actuators: for each of bits 2, 1, 0 (then left, right), a missing bit costs 1 MP. If MP is now
   exactly 0, it becomes 1 (bug 6).
4. Heat: MP -= heat / 5 (truncated).
5. Run: MP += MP / 2 (rounded down), + 1 if the undamaged walking MP is odd (bug 7). An undamaged
   'Mech runs ceil(1.5 x walk).
6. Heat 30 (shut down): MP = 0. MP is at least 0.

F:377c is set when legs reduced MP, F:4592 holds the heat loss (for the "inhibited" texts).
Example: the Locust walks 8, runs 12.

**A person** (`203b:2474(n)`): MP = 3 x byte 2 / 4 (rounded down), at least 3; halved if the
armour type (byte 0x0d) is above 1 (the heavier suits); at most 8. For n 12 and above: a flat 6
(bug 8).

### 3.3 The path and its cost

`203b:193b(fig)` (FUN_203b_193b) simulates the orders from the figure's position and writes the
path (up to 12 steps of one half-cell each, F:40b4) and the points left (F:3770):
- Each order in turn: if the figure is free to turn (a party figure not under computer control,
  or one with no heading yet) it faces the waypoint at once; otherwise the stepper turns it one
  eighth per step.
- While points remain: stop the order at the waypoint, or when the waypoint is taken by another
  figure. Otherwise one step of `1e31:0006` (a blocked step stays in place) and its cost.
- **Cost of the half-cell stepped onto**: 1. Not jumping and the tile below 0x40: if the tile's
  low four bits share a bit with the quarter of the cell the half-cell is in, 2, and 3 if the
  tile's group (high nibble) is 2. Tiles 0x40 and above cost 1; jumping always costs 1. The low
  four bits seem to say which edges of the cell the terrain covers (bit 0 top, 1 right, 2 bottom,
  3 left); this reading is a guess, the numbers are exact.
- A step is taken whenever at least 1 point is left, so the last step can overspend. A blocked
  step still costs its cell, so a blocked unit spends its points standing.
- The heading is put back afterwards: simulating does not turn the figure.

### 3.4 Moving

For each active figure, the next step of its path (F:0078 counts the steps taken):
- the end mark (2): nothing;
- (0, 0): nothing, and the counter does not move on, so the figure stops for the rest of the
  round (`1c67:0d7e`, which zeroes every path, stops everybody this way);
- the place taken (`1e31:16ab`): no move, and the same step is tried again next phase;
- otherwise the figure moves, its heading follows the step (0 north, clockwise), its walking
  script changes when the direction does, and its "steps so far" count goes up.

There are no draws in movement.

### 3.5 Choosing a shot

For figure s and weapon slot w (0-11), in this order:
1. The target is the signed byte F:3800 + 12s + w (ff none; bit 7 = already fired this round).
   A 'Mech at heat 30 or more has no target.
2. The weapon: a person always uses its character's weapon (byte 0x0b), for every slot. A 'Mech's
   slot 11 is the Kick (weapon 0x20), allowed only with both hip actuators (otherwise the slot's
   target is cleared); slots 0-10 are its weapons in item order (`1e31:10a2`: none, or no
   ammunition left, clears the target). A destroyed weapon is skipped.
3. A target that is no longer active is cleared.
4. The range band (3.6). Out of range: skip; the target is kept and tried again in later
   phases, and the figure may still fire another weapon this phase.
5. The direction from the attacker to the target. Line of sight (`1e31:1bfe`): blocked, skip.
6. From here the weapon fires.
7. **Skill practice** (party people, figures 4-11 only): F:d360 + s holds the skill last used and
   F:d358 + s a count of shots with it. Same skill: the count + 1, and when it wraps from 255 to
   0 the skill goes up by 1 if below 4. Another skill: the count starts again. (These bytes lie
   in the flag array, flags 0x50-0x5f.)
8. **Ammunition** (a 'Mech's slots 0-10): the slot's ammunition byte, unless ff (unlimited), goes
   down by 1, before the hit roll.

### 3.6 Range

**Distance** (`1e31:0bb5`, FUN_1e31_0bb5), from the party position, in half-cells on the linear
grid: d = min(|dX|, |dY|) + max(|dX|, |dY|) / 2 (rounded down). A straight line of 10 half-cells
is 5; a diagonal of 10 by 10 is 15. See bug 1.

**The aim point** (`1e31:0f24`): for a 'Mech target more than 3 away, the aim point moves one
half-cell toward the shooter in x, and 2 up if the shooter is above it (a 'Mech is three
half-cells tall).

**Bands**: S and M from the weapon's +0x0e byte, L its +0x0f (formats/mech.md). For a 'Mech
weapon other than the Kick, S and M are multiplied by 3; L never is. Short if d < S, medium if
d < M, long if d < L, else out (band 3). The Medium Laser: short below 12, medium below 21, long
below 30. A pistol: 3, 5, 9. The weapon's minimum range (the high nibble of +0x0d) is not used.

### 3.7 The to-hit number

The shot hits when 2d6 >= T. T is built in this order:

| step | term |
|---|---|
| 1 | 4 + 2 x band (short 4, medium 6, long 8) |
| 2 | Kick: T is replaced by 3 + the number of missing actuators among bits 0-2 of both leg nibbles (bug 2) |
| 3 | + the attacker's movement: the type of its first order + 1 (none 0, walk 1, run 2, jump 3) |
| 4 | + the target's movement: DG:2d1a[steps the target has taken so far this round] (0 for 0-2 steps, up to +4 for 10-12) |
| 5 | attacking 'Mech: + 2 if it has any sensor hit (byte 0x77); + 1 for each heat level reached: 8, 13, 17, 24. The weapon's heat (low nibble of +0x0d) is added to F:0092 + m here. |
| 6 | - the shooter's skill: character byte 4 + the weapon's skill number (+0x10). A 'Mech's shooter is its pilot (byte 0x79). |
| 7 | + the target's terrain F:32ae: >> 3 for a 'Mech target, >> 1 for a person (signed) |

There is no clamp: T 2 or less always hits, T 13 or more never.

The skill numbers: 0 Bow & Blade, 1 Pistol, 2 Rifle, 3 Gunnery (the 'Mech weapons, the people's
missiles), 4 Piloting (the Kick).

Because phases interleave, a target that has taken few steps so far is easier to hit early in
the round. Because party orders are moved up at each waypoint (3.1), a party 'Mech that has
reached its last waypoint has no orders and counts as standing still for step 3 (open
question 8).

### 3.8 Combat messages

Before the result, with DG:2e38 above 0, window 4 is cleared and a line is built: who fires
(name and "'s Mech", or "An enemy" + "human" / "Mech"), "kicks" or "uses a" + the weapon, " on ",
and the target (name, "an enemy Mech." / "human.", or "a spectator."). Level 1 (Brief) prints
only some parts: the weapon name, or "Kick". Texts DG:3e44 to DG:3ec0, all through `1e31:1dab`
(prints only when DG:2e38 is set). Then "Hit!" / "Missed!" (people: DG:3ed6 / DG:3ecd) or
"Hit " + the location name (DG:3ee7, names at DG:3242 / DG:324e) / "Missed!" (DG:3ede).
`1e31:1dcc` pauses after a shot: 12 x DG:015e frames, or until a key when DG:015e is 5 or more.

### 3.9 The hit location

Only for a 'Mech target. The target's facing is F:396c + t, or F:45b6 + t when that is ff (the
facing is parked there while a figure shoots). d = facing - direction (attacker to target),
taken mod 8, picks a row of the location table through DG:2d11:

| d | row of DG:2e40 | that row matches the board game's |
|---|---|---|
| 0 | 2 | rear column |
| 1, 2 | 1 | front column |
| 3, 4, 5 | 0 | left side column |
| 6, 7 | 3 | right side column |

The location is DG:2e40[11 x row + 2d6]. One 2d6 is drawn for every shot at a 'Mech, hit or
miss, even for weapons that do nothing to 'Mechs. The rows are not split evenly (one direction
gives the rear, three the left side); see open question 4.

### 3.10 Damage to a person

Target figure 4-11 or 16-23.
- Damage D and shots: a personal weapon (+0x0c bit 7) does D = the low nibble of +0x0b plus
  (high nibble) d6 (the dice are drawn before the hit roll), with +0x0c & 0x7f shots. A 'Mech
  weapon (and the people's missiles) does D = 127 per shot, with +0x0c shots (an LRM 20: 8). D
  is at least 1. The Rifle: 3d6; the Pistol: 3 + 2d6.
- The hit roll (2d6 >= T). A hit means every shot hits.
- Each shot: armour a (byte 0x0e). If a >= D / 2: D is halved and a -= the halved D. Otherwise
  D -= a and a = 0. Then hit points (byte 0x0f) -= D, to 0 at most. D is not reset between shots
  (bug 4).
- Hit points 0: killed (3.14).
- A miss may start a fire (3.13).

### 3.11 Damage to a 'Mech

Target figure 0-3 or 12-15 ('Mech m = t or t - 8).
1. The location 2d6 (3.9).
2. **The Kick**: D = the kicker's tonnage (byte 0x10) / 5, truncated (written into the Kick's
   damage byte, DG:3103). One r: r & 8 = 0 the left leg, else the right leg; the table result
   is replaced.
3. **A personal weapon**: nothing more. No hit roll, no damage, no text (bug 5).
4. **Clusters**: if +0x0c is above 1, a 2d6 c, and D = +0x0b x DG:2e5e[7c + +0x0c]. The whole
   cluster lands on the one location. An LRM 20 does 6-20, an SRM 2 1-2 (x 2).
5. **The hit roll** 2d6 >= T. A miss: D = 0, a fire check (3.13), and if the target stands on a
   blocking tile (F:45ce + t >= DG:0150) a mark 0x7f is added just above it (a shot into the
   building behind, a guess).
6. **Damage** while D > 0 and the 'Mech is not destroyed:
   - The Inferno (weapon 0x0b): no damage; the 'Mech burns for 3 rounds (F:d576 + m = 3, 3.15).
   - D not more than the location's value v: v -= D. A location that is internal structure
     (0x1c-0x23) gets a critical check (3.12). Head or centre torso structure now 0: the 'Mech is
     destroyed (`1d43:0a35`), and if it is figure 0 in a fight of kinds 1-3, every path is
     cleared (`1c67:0d7e`; bug 33).
   - D more than v: D -= v, v = 0; a critical check if internal; then D moves on (transfer):

| from | to |
|---|---|
| armour of a location (0x11-0x18) | its internal structure |
| rear armour LT, CT, RT (0x19-0x1b) | that torso's internal structure |
| arm or leg structure | the side torso's armour (left to LT, right to RT) |
| side torso structure | the centre torso's armour |
| head or centre torso structure | the 'Mech is destroyed |

   - An arm whose structure is used up exactly or emptied this shot sets F:3986 (a picture, 3.14).

### 3.12 Critical hits

`1e31:11ab(m, L)` (FUN_1e31_11ab), for every internal location damaged:
1. 2d6: below 8, 1 critical; 8-9, 1; 10-11, 2; 12, 3 (bug 3). "Critical!" (DG:315e) and
   F:4586 = 1.
2. If the location's structure is now 0: every item in its slots is destroyed, and an arm or leg
   loses all its actuators. No more draws.
3. Otherwise, until that many criticals have landed (a pick that finds nothing draws again):
   - Head: d6. 1 or 6: byte 0x78 = ff if not 0 (life support, a guess). 2 or 5: sensor hits + 1
     (at most 2). 3: the cockpit: head structure 0, the 'Mech destroyed. 4: the head's item slot.
   - Centre torso: d6 above 3: one of its two item slots. Otherwise another d6: above 3, engine
     hits + 1 (3 destroys the 'Mech); else gyro hits + 1 (2 destroys it). Destroyed here means
     centre torso structure 0.
   - Arm or leg: nothing left: done. d6 v of 3 or more with actuators left: actuator v (leg bits
     8, 4, 2, 1; arm 0x80, 0x40, 0x20, 0x10, tables DG:316a / DG:317a) is cleared, or the pick
     fails if it is already gone. v below 3 or no actuators: an item slot.
   - Side torso: no intact item: done; else an item slot.
   - An item slot pick (`1e31:163e`): if any item in the range is intact, one r: r & 3 is the
     starting slot, and the first intact item from there (wrapping) is destroyed.

### 3.13 Fires and map marks

On a miss only (`22e8:1e46`): if the target's terrain byte F:32ae is set, the weapon is the
Inferno, the Medium Laser, the Large Laser or the PPC, and the tile under the target (F:3750) is
0x10-0x3f: 2d6; 2-4 or 10-12 (1 in 3) adds mark 0x7c (fire, a guess) one half-cell left of a
person or above a 'Mech.

Marks go into a 64-entry ring (`203b:27c9`: F:d457 code, F:d497 area, F:d4d7 x, F:d517 y; write
index F:d557, the oldest overwritten). Codes: 0x7c fire, 0x7e a body, 0x7f a missed shot into a
building, 0x80 / 0x81 a wreck (two sprite sets). The ring is probably the map objects that
`1000:2a93` draws (docs/town_loop.md).

### 3.14 After the shot: kills, wrecks, pictures

`22e8:12c7` (FUN_22e8_12c7):
- **Presentation** (only when DG:2e3a is set and DG:014a): the shooter's fire pose, the shot (a
  missile sprite for the missiles, a line for the lasers and PPC, clipped to the map window), the
  hit reaction, sounds (`1000:19bf`, only with sound on), an explosion on a hit by weapon 7 or
  above. On a hit, for some weapons, a place picture (`1000:48b7(n, 0)`, one draw): 1 for missiles
  at a 'Mech, 4 or 7 for a party 'Mech's lasers, 3 for an enemy hitting figure 0.
- **A person killed**: "Killed him!" (DG:41fc); destroyed, inactive, the record emptied, a body
  mark 0x7e; every target byte aimed at him cleared. Character 0 or 1: DG:014a = 0, the fight ends
  at once (open question 6).
- **A 'Mech destroyed** (`1d43:0a35`): inactive, destroyed flag, its first letter kept in
  F:323e + m, record byte 0 = ff, targets at it cleared, "Mech is destroyed!" (DG:2b6d). A party
  'Mech: "Men eject!" (DG:2b81, always, even when nobody is inside); its pilot and second
  occupant go on foot at its place (the second one a half-cell right). Enemy crews do not
  eject. A wreck mark 0x80 / 0x81 one half-cell up and left, sound 8.
- **The spectator** (F:e48e, figure 13): always in line of sight, never targeted by the computer,
  no "destroyed" text. Destroying it: a message box (DG:4209, a key), then DG:4235, 'Mech
  records 6 and 7 become URBANMECHs (27e8:065b) as figures 14 and 15 at x 0x097d, F:e48e = 0,
  flag 23 = 1 (the exit zone opens, 2.8). Destroying 'Mech 4 also removes 'Mech 5 (figure 13);
  destroying 'Mech 5 patches the map (`1d43:0c72(0)`).
- After the shot: if an arm was destroyed and graphics are on, picture 0x10 (one draw); if a
  critical hit Jason's 'Mech and graphics are on, picture 5 (one draw). The pause, the target
  byte gets bit 7 (fired).

### 3.15 The end of the round

`1e31:0c63` (FUN_1e31_0c63):
1. Every target byte: bit 7 cleared; a target no longer active is cleared.
2. **Heat**, for each 'Mech that exists (party figures 0-3, enemy 12-15):
   h = movement (first order type + 1; a jump of more than 3 steps counts the steps)
     + 5 x engine hits (byte 0x75)
     - heat sinks in the engine (byte 0x26)
     - intact heat sink items (0x22)
     + the weapon heat of this round (F:0092 + m).
   Heat += h. Burning (F:d576): + 6 and one round fewer. Standing in water (terrain byte set and
   the tile below 0x10, a guess): - 4. Clamped to 0-30. F:0092 = 0.
3. Characters whose record is empty (dead): the figure position ffff (not for character 0);
   enemy people's sprite base fe; character 0's figure keeps sprite base 0x96.

Heat 30 is the only effect beyond the to-hit and MP penalties: the 'Mech cannot move or fire
until it cools. There is no shutdown or ammunition roll.

## 4. The computer planner: `1e31:03ab(fig, show)`

FUN_1e31_03ab, 2058 bytes. It plans one figure for either side. **It draws no random numbers:
the planner is deterministic.**

1. **Set-up**: the party position goes to the figure; its orders are cleared. A party figure gets
   F:3994 = 1 (the computer now moves it).
2. **Mode**: a 'Mech runs if its heat is 0, else walks; never jumps. People walk. Movement points
   as in 3.2 (bug 8 for enemy people).
3. **Target**: the nearest active figure, from lists tried in order, a later list only if the
   earlier ones gave nothing:

   | planning | first | then |
   |---|---|---|
   | a party 'Mech | enemy 'Mechs 12-15 | enemy people 16-23 |
   | an enemy 'Mech | party 'Mechs 0-3 | party people 4-11 |
   | a party person | enemy people 16-23 | figures 12-19 ('Mechs and people) |
   | an enemy person | party people 4-11 | figures 0-7 ('Mechs and people) |

   Ties keep the earlier figure. Never the spectator. The enemy ignores the traitor on foot.
4. **Reach R**: a person: the short limit S of its own weapon. A 'Mech: 4 x the smallest short
   limit among its intact weapons (bug 9; quirk 10). Then line of sight to the target.
5. **A person facing a 'Mech** runs: F:3992 = 1 for a party person, 0 for an enemy one (bug 13);
   F:374c = 1 for an enemy person (2.10). The waypoint is 6 half-cells away from the target on each
   axis (unchanged on an axis where they are level). It still fires at the target.
6. **Movement order** (only the first order is written, one waypoint):
   - R = 0 or no line of sight: straight to the target's position (the stepper stops next to it);
   - farther than R: a stop point, d - R diagonal or straight steps from the figure in the
     target's direction, the direction fixed once (quirk 11);
   - in reach with line of sight: no order.
7. **Weapons**: a person: its one slot at the target (not the traitor). A 'Mech: in contact with
   an enemy 'Mech it can see (|dX| <= 3, |dY| < 2): it stands, faces the target and its facing
   is parked. Shut down (heat 30): no order, nothing fires. Otherwise every weapon slot 0-9 that
   has ammunition gets the target, and the Kick (slot 11) always (the round drops it without hips).
   Slot 10 is never set. Range band, heat and minimum range are ignored: it fires everything.
8. The path is simulated (`203b:193b`) and, with show, drawn.

`203b:1482(first)` plans the twelve figures from `first` (0 or 12) that are active.

## 5. Every random draw, in order

Draws are r unless written d6 or 2d6 (each of those is one or more raw draws).

**Before a random encounter**: the game tick's r (1.1).

**The encounter set-up** (`15ab:0d3d`, kind 0):
1. dx, its sign, dy, its sign: four r.
2. For each enemy character 8-15: r (& 1); if a soldier: 7 r (weapon), 2d6, 2d6, 7 r (skills),
   r (armour type), 2d6, 2d6.
3. For each enemy 'Mech 4-7: r (& 1); if 1 and the matching party 'Mech exists: r (template).
4. Declined "Engage": r (& 3).

**A mission's set-up**:
1. Mode 1: r (& 7) for the spot. Mode 3: r (& 3) for the count. Mode 6: r (& 1) for the raid.
2. `17dc:0d49`: x, y: two r. Per enemy 'Mech: flag 03 above 4, five r; above 5, three r. Per
   enemy person: six r, 2d6, r (weapon).

**Each round**:
1. Orders and planning: none.
2. Flee chosen: r. (Not in the raid's walk-out case.) Computer control, F:3992 set and no party
   'Mech: r.
3. The 12 phases: the shots below, in phase order and slot order.
4. Enemy people running, no enemy 'Mech left: r.
5. The raid's second story line: one r (the picture).

**Each shot** that passes range and line of sight:
- At a person: personal weapon: (high nibble) d6; 2d6 to hit; a miss that meets the fire
  condition: 2d6.
- At a 'Mech: 2d6 location; Kick: r; personal weapon: stop; cluster: 2d6; 2d6 to hit; a miss
  that meets the fire condition: 2d6; on a hit, for each internal location damaged: 2d6 for the
  count, then per critical d6 (one or two) and possibly r for an item slot, repeated for failed
  picks.
- With graphics on (DG:2e3a): one r for the picture on a hit with a picture number; one r if an
  arm was destroyed; one r if a critical hit Jason's 'Mech.

**After the fight**: repairs: N r (only with N > 0 and I > 0); salvage: two r per pick; loot: one
r per beaten enemy person (only when something is shown); then, for modes 5-7, an r for Gunnery
and one for Piloting when each is below 4.

**Settings that change the draws**: the graphics setting DG:2e3a adds up to three draws per shot.
The computer-fights answer F:009e decides which flee draw can happen. The message level and the
message speed change no draws.

## 6. Original bugs and quirks

The rebuild keeps each one by default and offers a fix behind a switch that is off by default
(the mw3-re FIX practice). The switch names are proposals. "Probable" means it reads as a
mistake; "quirk" means it may be intended.

| # | what happens | where | fix switch |
|---|---|---|---|
| 1 | Distance is min + max/2, not the usual max + min/2: straight lines count half, diagonals relatively long. Quirk; it may be meant. | `1e31:0bb5` | `fix_distance` |
| 2 | The Kick's to-hit counts missing actuators of 'Mech record s, the figure number; for an enemy kicker (figures 12-15) that reads past the eight records. Probable. | `22e8:000c` to-hit, `1e31:1b44` | `fix_kick_actuators` |
| 3 | Every damaged internal location gets at least one critical; the board game gives none below 8. Probable. | `1e31:11ab` | `fix_critical_count` |
| 4 | Against people, D is not reset between the shots of one weapon: armour halving carries on. Probable. | `22e8:000c` | `fix_person_damage` |
| 5 | A personal weapon at a 'Mech: no roll, no effect, and the hit flag passed to the animation is left from the last shot (unset at first). Presentation only. | `22e8:000c`, `22e8:12c7` | `fix_stale_hit_flag` |
| 6 | Actuator losses: MP is lifted to 1 only when exactly 0, so -1 ends as 0. Probable. | `203b:22bc` | `fix_actuator_min` |
| 7 | The run bonus +1 tests the undamaged walking MP for oddness. Quirk. | `203b:22bc` | `fix_run_parity` |
| 8 | Enemy people's MP: the planner passes figure - 8, so figures 16-19 use party characters 4-7 and 20-23 a flat 6. Probable. | `1e31:03ab`, `203b:2474` | `fix_enemy_foot_mp` |
| 9 | The 'Mech reach uses the short limit of the next weapon in the table (item code, not item code - 1). Probable. | `1e31:03ab` | `fix_planner_reach_index` |
| 10 | The reach is 4 x S where the bands use 3 x S. Quirk. | `1e31:03ab` | `fix_planner_reach_scale` |
| 11 | The stop point steps diagonally in a fixed direction, so it can overshoot the target's row or column. Quirk. | `1e31:03ab` | `fix_planner_stop_point` |
| 12 | The computer fires every weapon at every range and always tries the Kick. Behaviour; kept, no switch. | `1e31:03ab` | none |
| 13 | Planning an enemy person against a 'Mech sets F:3992 = 0, wiping a retreat set by a party person; planning a party person against a 'Mech sets it to 1, which the mission driver reads as fleeing. Probable. | `1e31:03ab`, `17dc:0629` | `fix_retreat_flag` |
| 14 | Under computer control, an auto-retreat value of 2 is later read as "left the raid area". Probable. | `203b:000a` | `fix_auto_retreat` |
| 15 | The flee draw is made in kinds 2 and 3, where fleeing is refused. Kept for the draw order. | `203b:000a` | none |
| 16 | A waypoint in area rows 8-15 is never recognised as a duplicate (sign extension), so it is added again. Harmless. | `203b:1c1f` | `fix_waypoint_compare` |
| 17 | A 13th waypoint would be written into the next figure's order list. Probably unreachable. | `203b:1c1f` | `fix_order_bound` |
| 18 | The path is written without a bound; more than 12 steps would spill into the next figure's path. | `203b:193b` | `fix_path_bound` |
| 19 | Scan: for a 'Mech without sensors the "no enemy humans" check is skipped and the list is opened empty. | `203b:2591` | `fix_scan_sensors` |
| 20 | The reachability test before "Engage" changes figure 0's heading. | `203b:28db` | `fix_probe_heading` |
| 21 | An enemy person whose target byte is ff is measured against figure -1 (F:4002 / F:4034) when the people run. | `203b:000a` | `fix_flee_target` |
| 22 | Repairs with Tech 2+ mend every destroyed heat sink, even when the wrecks had none. Probable. | `15ab:0002` | `fix_heat_sink_count` |
| 23 | The weapon count for item 0x20 (SRM 6) is never cleared: it starts with leftover stack data. Probable. | `15ab:0002` | `fix_weapon_count` |
| 24 | Scrap money is multiplied by the best tech's character number, not his skill; with Jason as the best tech there is none. Probable. | `15ab:0002` | `fix_scrap_multiplier` |
| 25 | A salvaged Jenner or UrbanMech gets sprite base 0x92; the encounter table gives them 0. | `15ab:04f9` | `fix_salvage_sprite` |
| 26 | The wreck search includes figures 4-11 (killed party people), which index the wrong records, and never ends if no flag is set. | `15ab:04f9` | `fix_wreck_search` |
| 27 | Loot reads character s + 12 (off the table, into 'Mech 0) to decide whether weapons were found; a dead party member alone gives the pocket text and 2 C-Bills. Probable. | `15ab:094b` | `fix_loot_index` |
| 28 | Encounter soldiers' armour points are 2d6 + 2d6 whatever the type (also with none). Quirk. | `15ab:0d3d` | none |
| 29 | Mode 3 can ask for 5 or 6 enemy 'Mechs; `0d49` has no limit, so records 8-9 (past the table) and figures 16-17 (people's slots) are used. Effect in play not checked. | `17dc:0629`, `17dc:0d49` | `fix_mission_count` |
| 30 | The difficulty's destroyed slots are picked from 0x34-0x53 only: the first left-arm slot, the second centre-torso slot and the head slot are never hit. Quirk. | `17dc:0d49` | none |
| 31 | Reloading the areas around the party does not wrap columns: column 0's left neighbour is column 15 of the row above. | `203b:2835` | `fix_area_wrap` |
| 32 | The scan list never ends if no figure in it is active; a direction of -1 reads before the direction names. | `15ab:1467` | `fix_scan_list` |
| 33 | Jason's 'Mech destroyed in a mission stops all movement only when the damage ended exactly in the head or centre torso, not when it overflowed. | `22e8:000c` | `fix_stop_on_loss` |
| 34 | Only ten ammunition bytes: an 11th weapon would use the walking MP byte as its ammunition. No template has more than nine weapons. | `1e31:10a2`, `22e8:000c` | none |

The switches are `game/fixes.h`. Each is off by default, except where the original reads or
writes memory the rebuild does not hold, or never returns: bugs 2, 17, 18, 21, 23, 26, 29 and 32
take the fixed path by default and log it. Bug 5 is kept, but the first shot of a session (whose
flag the original leaves uninitialised) counts as "missed" and is logged. Bugs 12, 15, 28, 30 and
34 have no switch, nor does the unreachable encounter-table path (DG:13e2).

## 7. The copy-protection check

`1c67:08a8` (FUN_1c67_08a8) runs on entering the training center (entrance 0) while flag 14 is 0 and
no demo plays (docs/scripts.md, "Entering a place"). It is the original's copy protection: a quiz
against the 'Mech pictures in the manual.

1. BTSTATS.CMP (name at DG:27e6) is loaded and shown (not when F:4594). A message (DG:27f2) says a
   written test comes first; a key.
2. Three questions. Each picks a picture number r mod 10 (signed division), drawn again until it
   was not asked before; a box is drawn round picture i (DG:28c4 / DG:28d8); a menu of 20 'Mech
   names (menu 0x25, the list at DG:4de2, after DG:286c) asks which it is; the answer is compared
   with DG:28ec[i]. Any wrong answer sets the fail flag.
3. Failed: text DG:2886, flag 14 = ff (no new try for 255 ticks), a key, and the place is not
   entered.
4. The panel is redrawn.

A pass sets nothing, so the quiz is asked on every visit; after a failure flag 14 counts down
from ff in the town's ticks and the quiz comes back when it reaches 0.

**The copy we read is modified here.** In the BTECH.EXE we read, this check has been altered so
that it always passes. That is not the original's behaviour, so this page describes the check as
the program was built and nothing here relies on the altered copy. Following docs/rules.md (rule 3),
the rebuild has no protection check and nothing in it can fail.

**The draws are kept.** The original picks the three pictures (step 2) before it compares any
answer, so the draws are the same whether the player passes or fails. The rebuild makes them on
each visit while flag 14 is 0 (`game/training_quiz.h`, `draw_quiz_pictures`) and shows nothing for
now; how the scene looks (for example the stats pictures with the answer given in our own words)
is still to be decided. The original also waits for input here (a key after the message, then three menu
answers), which the rebuild does not, so a rig script that enters the training center must leave
them out on the rebuild's side.

## 8. Open questions

1. How the Arena's enemy 'Mechs are made. `0d49`'s F:e48e code and the encounter table DG:13e2 sit
   in the branch for bit 0x80 of its first argument, which none of its five calls passes. Mode 8
   asks for no 'Mechs. `17dc:1c9b` sets the eight characters' 'Mech slot bytes to 3 (meaning
   unknown).
2. F:32ae (terrain: to-hit, the course count, water cooling) and F:3750 (the tile under a figure):
   exact meanings and who sets them.
3. The nine bytes P:07a4..07ac whose bit 0x80 blocks salvage.
4. The location rows: d = 0 gives the rear column, but three directions give the left-side column
   and two the front. Either our reading of the facing (F:396c) or of the direction (`287f:0971`,
   not read) is off, or the original is lopsided. To check in the running original.
5. The meaning of the mark codes 0x7c and 0x7f, and what draws the mark ring.
6. Why character 1's death ends the fight like Jason's (DG:014a).
7. F:396c + i and F:397c + i at the end of the fight: which figures 0-7 is meant for.
8. A party 'Mech that reaches its last waypoint has no order left, so it counts as standing for
   to-hit and heat; enemies keep their order. This follows from the code as read; check in play.
9. Flag 24's values and where it is set, besides the traitor's 0x7f.
10. `1c31:000a` (healing after a fight), `17dc:15e6` (offering a weapon), `15ab:18e8`, `1000:0e4b`,
    `287f:1df8`, `287f:1da8`: not read.
11. The order of `1000:2da8`'s two arguments in the raid (map 4 into slot 0x0b, or the reverse).
12. Whether the menu `2656:0b5e` can return -1 (Esc).
13. 'Mech byte 0x78, F:4586 / F:3986 readers besides those above, DG:015e and DG:0150 (where they
    are set).
14. The three character bytes 1-3 (by use: byte 1 sets the hit-point maximum, byte 2 the walking
    speed on foot); their names.

## Functions

| address | Ghidra | what |
|---|---|---|
| `17dc:0629` | FUN_17dc_0629 | the mission driver, modes 0-9 |
| `17dc:0d49` | FUN_17dc_0d49 | opponent set-up for missions |
| `17dc:134b` | FUN_17dc_134b | the party position back on Jason's figure |
| `17dc:1c9b`, `1b41` | FUN_17dc_1c9b, FUN_17dc_1b41 | the Arena 'Mech set-up and its undo |
| `19b8:152f` | FUN_19b8_152f | Jason alone (hides the rest), then mode 9 |
| `1c67:08a8` | FUN_1c67_08a8 | the copy-protection quiz |
| `1c67:0d7e` | FUN_1c67_0d7e | clears every path |
| `1d43:0004` | FUN_1d43_0004 | Use Weapons |
| `1d43:07cb` | FUN_1d43_07cb | the target picker |
| `1d43:0a35` | FUN_1d43_0a35 | a 'Mech is destroyed; crews eject |
| `1e31:03ab` | FUN_1e31_03ab | the computer planner |
| `1e31:0bb5` | FUN_1e31_0bb5 | distance |
| `1e31:0c63` | FUN_1e31_0c63 | end of round: heat, cleanup |
| `1e31:0f24` | FUN_1e31_0f24 | range band |
| `1e31:10a2` | FUN_1e31_10a2 | a 'Mech's i-th weapon |
| `1e31:1122` | FUN_1e31_1122 | damage transfer |
| `1e31:11ab` | FUN_1e31_11ab | critical hits |
| `1e31:163e` | FUN_1e31_163e | destroy an item slot |
| `1e31:1bfe` | FUN_1e31_1bfe | line of sight |
| `15ab:0002` | FUN_15ab_0002 | repairs and scrap |
| `15ab:04f9` | FUN_15ab_04f9 | salvage |
| `15ab:094b` | FUN_15ab_094b | loot |
| `15ab:0b5e` | FUN_15ab_0b5e | "computer fights for you?" |
| `15ab:0d3d` | FUN_15ab_0d3d | the random encounter's set-up |
| `15ab:1467` | FUN_15ab_1467 | the scan list |
| `203b:000a` | FUN_203b_000a | the fight |
| `203b:1482` | FUN_203b_1482 | plan twelve figures |
| `203b:14c3` | FUN_203b_14c3 | the orders menu |
| `203b:1774` | FUN_203b_1774 | show a figure and its path |
| `203b:193b` | FUN_203b_193b | simulate the orders: path and cost |
| `203b:1c1f` | FUN_203b_1c1f | enter a waypoint |
| `203b:2231` | FUN_203b_2231 | Kick |
| `203b:22bc` | FUN_203b_22bc | a 'Mech's movement points |
| `203b:2474` | FUN_203b_2474 | a person's movement points |
| `203b:24f0`, `2556` | FUN_203b_24f0, FUN_203b_2556 | the message and graphics questions |
| `203b:2591` | FUN_203b_2591 | Scan Unit |
| `203b:273d` | FUN_203b_273d | is an item slot's location intact |
| `203b:27c9` | FUN_203b_27c9 | add a map mark |
| `203b:2835` | FUN_203b_2835 | reload the areas around the party |
| `203b:28db` | FUN_203b_28db | can Jason reach an enemy |
| `22e8:000c` | FUN_22e8_000c | one round |
| `22e8:12c7` | FUN_22e8_12c7 | a shot's presentation, kills and wrecks |
| `22e8:1e46` | FUN_22e8_1e46 | the fire check on a miss |
| `24d3:0027`, `0187`, `01c2` | in FUN_24d3_0004 | training specials 1-3 |
