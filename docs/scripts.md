# The place scripts

What happens inside a place: the interpreter for the .BLD scripts (the file is in
[formats/bld.md](formats/bld.md)). `tools/bld.py --data DATA ORIGINAL/CITADEL.BLD` (DATA: the imported data folder) lists a script with these
names and its text IDs.

## Entering a place (BTECH.EXE 17dc:0008)

Answering Yes at an entrance calls `17dc:0008(n)` with the map's place number:

1. Sets flag 0a (F:d316) to 1, and stores n at F:4584 (used on leaving, below).
2. Maps n to a script number through the map's own table (the .MTP's `place_scripts` at file
   offset 0x205, loaded at F:4602; docs/formats/mtp.md). Numbers 0x12 and 0x16 and up are used
   as they are. On map 1, entrance 1 is the Citadel.
3. Asks for the right game disk (1000:28cc). The rebuild drops this: there are no disks.
4. **The training center's quiz.** For entrance 0, while flag 14 (d320) is 0 and no demo is
   playing (F:3938, F:458c), `1c67:08a8` runs first; if it returns nonzero, the place isn't
   entered at all. A failed quiz sets flag 14 to ff, and so does TRAINING.BLD as each mission
   starts; it counts down in the town's ticks, so the quiz comes back on the first visit after it
   runs out (docs/combat.md, "The copy-protection check"). The rebuild has no check: it makes the
   quiz's draws and, only with `--show-quiz`, shows the scene.
5. Loads and deciphers the script (`17dc:1d30`), shows the place's picture if it has one (byte
   table DGROUP:141a by script number, through `1000:48b7` with mode 1, below), and runs the
   script from offset 0 until `end`.
6. Runs special 14 (the end-of-visit bookkeeping; see Specials).
7. Two follow-on places:
   - leaving the garage (8) with flag 30 set and flag 0e set, and flag 32 clear, shows a text
     (DGROUP:1384, 13b6) and runs place 12 (VIEWDISK);
   - leaving place 13 (BARRACK2) with flag 0e set runs place 12 too.
   In both cases 27e8:0064 is set to 0c.
8. Puts the screen back: window 4 cleared, window 3, the panel redrawn (`1000:4cac(1)`).
9. Leaving place 14 (ENTRANCE) with flag 36 set clears the 8 party records' first byte and calls
   `1b5d:0004`.

## The machine

- The script, deciphered, with a program counter (pc) from offset 0.
- **Flags.** A byte array at F:d30c, indexed by a signed byte, all 0 at a new game except
  the money floor. The scripts and the rest of the program share them (src/game/flags.h):

  | flag | byte | what |
  |------|------|------|
  | 04 | d310 | outdoors (clear on a town map) |
  | 08 | d314 | the course chosen (special 05) |
  | 09 | d315 | special 05's answer: 1 enrolled |
  | 0a | d316 | set on entering a place |
  | 0d | d319 | which place, for "talk to others" |
  | 0e | d31a | members besides Jason (special 14) |
  | 14–16, 1d, 29, 37–39 | | timers the game tick counts down (docs/town_loop.md) |
  | 17 | d323 | the money step's countdown |
  | 2d, 2e | d339, d33a | the let-out case of "talk to others" |
  | 33–34 | d33f | the money floor, a word: 50 at a new game |
  | 4f | d35b | how often a place's picture shows |
- **The current text window and its cursor** (docs/text.md).
- **C-Bills**, F:d370 (32-bit).
- **The party's map place**, P:a44b (x) and a44d (y).

Execution: read the byte at pc and step past it. An opcode E4–FF runs; any other byte is stepped
over. It loops until `end`. Operands are read in order after the opcode. A byte operand is
sign-extended (cbw) wherever the table says signed. A word operand is little-endian, read as an
unsigned 16-bit value. Jumps go to an absolute script offset.

## Opcodes

| op | name | operands | what it does | state |
|----|------|----------|--------------|-------|
| E4 | sound | b n | plays sound effect n (1000:19bf; only with sound on, DGROUP:015c) | |
| E5 | earn | w amount | C-Bills += amount (sign-extended to 32 bits); redraws the C-Bills (`1e31:1fdf`) | d370 |
| E6 | set_party_place | w x, w y | puts the party at map place (x, y) | a44b, a44d |
| E7 | if_party_x | w x, w to | if the party's x == x, goto to (only x is compared) | reads a44b |
| E8 | if_random | b mask, w to | r = random (287f:0bc0); if r AND mask (sign-extended) ≠ 0, goto to | random |
| E9 | new_member | b skill | a new member joins (`19b8:0d58`, below) | party records |
| EA | picture | b n, b mode | shows place picture n, O<n>.ANM, in the picture box (`1000:48b7`); not while a demo plays | |
| EB | if_d451 | w to | if F:d451 ≠ 0, goto to | reads d451 |
| EC | if_d450 | w to | if F:d450 ≠ 0, goto to | reads d450 |
| ED | if_skill | b skill, b level, w to | if any party member (record byte 0 ≠ ff) has skill byte `skill` ≥ level (signed), goto to | reads c614+ |
| EE | pay | w amount | C-Bills −= amount, or to 0 if it is more than they have; redraws the C-Bills | d370 |
| EF | if_cbills | w amount, w to | if C-Bills ≥ amount (unsigned 32-bit compare), goto to | reads d370 |
| F0 | cursor | b col, b row | the cursor to (col, row) in the current window (signed) | 3748, 374e |
| F1 | add_flag | b f, b n | flag f += n (byte, wraps) | flags |
| F2 | pause | | waits 50 ticks (`273d:0006`), then empties the keyboard buffer (`1000:2a2b`) | |
| F3 | switch_flag | b f, words… | goto the word at table + 2·flag f (signed) | reads flags |
| F4 | set_flag | b f, b v | flag f = v | flags |
| F5 | special | b n | runs special action n (below) | many |
| F6 | if_yes | w to | prints the Yes/No question (`1000:1a13(1)`, Yes first); if Yes, goto to | |
| F7 | if_flag | b f, w to | if flag f ≠ 0, goto to | reads flags |
| F8 | goto | w to | goto to | |
| F9 | menu | b m, words… | menu m (`2656:0b5e`, below); goto the word for the chosen item | menu record +6 |
| FA | frame | b style | draws frame style `style` round the current window (`2656:0004`, docs/text.md) | |
| FB | wait_key | | waits for a key (`273d:0259`) | |
| FC | print | text, NUL | prints the text in the current window, word-wrapped (`2656:03f5`, docs/text.md) | cursor |
| FD | clear | | clears the current window, cursor to its corner (`2656:0388`) | cursor |
| FE | window | b n | makes text window n current (`2656:0281`) | window |
| FF | end | | stops the script; back to 17dc:0008 | |

Every original script starts `window 6, clear, frame 0`: the big text box on the right.

### The menu (BTECH.EXE 2656:0b5e)

The script prints the menu's words first, as ordinary text. The menu record (385b:0092, imported into tables.json: formats/tables.md) says where
the items are: item i is row first_row + i of the current window, `width` cells wide. Then:
- The remembered item (record +6) is the start. It is reset to 0 if it is out of range, or while
  a demo plays or records.
- The bar is drawn by XORing colour `highlight` (14) over the item's 8 pixel rows (`287f:2b87`),
  and drawing it again takes it away.
- Up and down move the bar, wrapping at the ends. The key goes through `2656:0d1d`, which also
  maps the keypad and some letters to the arrows.
- Enter or space chooses.
- Esc returns the remembered item without remembering anything new. Any other choice is
  remembered in record +6.

So the bar comes back where it was the next time that menu is shown, in this visit or a later
one: entering a place does not reset the table (the rig's `citadel_*` cases compare all 41
remembered items). The table is game state, and the talk special changes menu 20's item count.

### The Yes/No question (BTECH.EXE 1000:1a13)

This is the same question as at an entrance (already rebuilt, docs/text.md), with Yes as the
starting answer.

### A new member (BTECH.EXE 19b8:0d58)

The first free record of the 8 (byte 0 = ff) gets:
- the next name number (F:d456, from 2 to 9 and round again);
- three random attributes (`1000:19dd`), and random 0/1 for each of 7 skills;
- skill `skill` set to 3 (a mech-tech also loses some of an attribute);
- a 'Mech slot if one is free.

Then it prints who joined, redraws the panel, and waits for a key. With flag 26 (d332) clear and
an odd random number, it also marks that member for something later (d330–d333). Not rebuilt
yet: neither the Citadel nor the training center uses it; the rebuild logs it.

### The picture (BTECH.EXE 1000:48b7)

`picture n, mode` (and step 5 above) shows O<n>.ANM (docs/formats/anm.md) in the picture box, the
upper left 88×88 at (8, 8). One random number is always drawn. The picture shows when
(r AND mask) is 0, the mask from DGROUP:0ac4 {03, 0f, 3f} by flag 4f. It always shows when mode
is 2, or when n ≠ 0x10 and n is 0, 2, 6 or above 7. For n above 0x10 the file must exist, otherwise
window 4 is cleared. Picture 8 plays 7 times. In mode 0 it plays and goes: sound 12 after
picture 0, a wait of 60, window 4 cleared. Otherwise the last frame stays.

## Specials (F5 n)

`24d3:0004` is one big switch, with its jump table at 24d3:1762. "Files" says which scripts use
each one.

| n | code | files | what it does |
|---|------|-------|--------------|
| 01 | 24d3:0027 | TRAINING | the 'Mech walks in and the mission in flag 00 runs (docs/combat.md 1.2, 1.3) |
| 02 | 24d3:0187 | TRAINING | after the course: by the first letter of 'Mech record 0, DGROUP:48b3 for an L (`course.result_locust`), else 48df for a W or 48e4 for any other letter (the 'Mech's name) and then 48ee (`course.result_*`) |
| 03 | 24d3:01c2 | TRAINING | the 'Mech is handed back: record 0 emptied (byte 0 = ff), Jason on foot (character 0 byte 0x0c = 8), the party at (0x0c2e, 0xc076) outside the training center. Unless flag 04 is set, marks 0x7c and 0x7d (fires) lose their code, area and x (F:d457, d497, d4d7), keeping y |
| 04 | 24d3:0231 | CITADEL | prints the three courses' prices: for skill i (0–2), 125 × Jason's skill i + 75, right-aligned to column 39 on row 5 + i |
| 05 | 24d3:02a5 | CITADEL | takes the course in flag 08: prints DGROUP:491a and the level and course names (tables DGROUP:4ea2, 4eb6). At level 4 it prints 4948 and sets flag 09 = 0. With enough C-Bills: flag 09 = 1, the skill + 1, the price paid. Otherwise it prints 4999 and sets flag 09 = 0. |
| 12 | 24d3:0aea | ten places | "talk to others": `17dc:17b9`, below |
| 14 | 24d3:0af2 | ten places, and after every visit | flag 0e = how many of the party records 1–7 are in use |
| 23 | 24d3:0cf8 | healing (`1c31:000a`) | flag 19 = 1 if any party member's hit points differ from 10 x B, else 0 |
| 43 | 24d3:157e | TRAINING | after mission 5 is won (+1672, with flag 2d = 1): townsperson 0 is put at the Lounge's entrance (MAP entrance 7; record +0/+2 and figure 16), inside (+9 = ff), its route 0x70 (last the Lounge, next entrance 0) and its target entrance 0, the Training Center. Talk to others in the Lounge lets it out (below) |
| others | see the table above | the other places | read with those places |

Specials 6–11, 13, 15–22 and 24–47 (but 43) belong to the other places and are read with them. The rebuild logs an
unrebuilt special and goes on.

Special 05's texts are imported from BTECH.EXE as `course.*` (docs/import.md).

### Talk to others (BTECH.EXE 17dc:17b9)

1. The place is the first i with F:4602[i] == flag 0d.
2. Clears the window. The townspeople inside are those waiting (wait ≠ 0) whose route's place
   ((route >> 4) & 7) is this one (docs/townspeople.md).
3. Nobody: `talk.nobody`, wait for a key.
4. Otherwise `talk.you_see`, each one's name and `talk.line_end`, then `talk.done`, and menu 20
   with its item count set to the number listed + 1. Done returns.
5. The let-out case: with flag 2d set, place 7 and the first one chosen, flag 2e = 1 and walker 0
   stops waiting. Otherwise flag 2e = 0, and the one chosen says why it is here
   (`talk.hello`, `talk.reason.<flag 0d>`) and where it goes next (`talk.going_to`, the place's
   name from the map, `talk.end`), then a key. Outdoors it is a random rumour instead (not
   rebuilt: there are no townspeople outdoors yet).

## Original quirks

- **WEAPON's unreached block.** A `goto 0220` at 015d always jumps over 0160–021f, which has
  another line from the shopkeeper. The player never sees it. It is kept (not imported, not
  shown); a fix would need to know what the jump was meant to test.
- **if_party_x** compares only x. No original script uses E7, so nothing shows the effect.

## After the raid: what the scripts offer

Read from the scripts' control flow (`tools/bld.py`) and the specials' code; not yet run in the
rebuild except where a section above says so.

**Map 11** (the ruined town, area 0xcc) has map 1's entrance table except entrance 0, which runs
BARRACK2 (0x13) instead of TRAINING. But only entrance 0 has a position (0x0c11, 0xc009): the
other fifteen are (0, 0), which no step lands on, so BARRACK2 is the only place the party can
enter there. Its money stays as the raid left it, and town ticks still draw random numbers. What
the same scripts offer where their entrances are open (map 1, and the other towns that use them):

| place | what it offers | specials |
|---|---|---|
| BARRACK2 | one of two texts by the party count (special 14, flag 0e) | 0e |
| CITADEL | the skill courses (special 4's prices, special 5) | 04, 05, 0c |
| COMSTAR | the money and a list of three 32-bit amounts at F:d374 (special 6); menus over them (7, 8) | 06, 07, 08, 0c |
| WEAPON | buying a weapon: special 0b takes the price (DG:4f44, by the choice) if the money covers it and sets flag 0b | 0a, 0b, 0c |
| ARMOR | buying armour: special 09, the same with DG:4f26; specials 1f / 20 not read | 09, 0a, 0d, 0e, 1f, 20, 0c |
| REPAIR | apprenticeship: alone, special 10 / 0f, a Tech course for 500 (Tech + 1 once, character byte 0x10 bit 0); with a party and flag 18, a recruit (`new_member 5`, flag 20 once, or Tech 3+ and a random bit). Repairs (12), the speed shop (13, 14: the upgrades, 19b8:0925) and ammunition (2c) need a 'Mech: special 11 counts the 'Mech records in use, special 1d (with flag 18) the letters at F:d452..d455 | 0a, 0c-0f, 10-14, 1d, 2c |
| BARRACKS, LOUNGE | talk, and the flags of the Lounge's let-out (2d, 2e, 2f) | 0c |

Special 0a prints the money (F:d370, 32-bit). At the end of the unpoked chain Jason has 170.
None of these places pays the party: money comes from fights (scrap, 2.15) and the town's money
step (docs/town_loop.md), whose 15 C-Bills come only in a town; the ruined town counts as outdoors
(flag 04), where the step changes only the three amounts.

**Where the story goes next.** The recruits are HOSPITAL (`new_member 6`; maps 2-9), REPAIR
(`new_member 5`, which needs flag 18, set only by GARAGE: map 2) and JAIL (`new_member 4`; map 3).
The only code that writes a new 'Mech record outside the fights and the shops is reached from
ARENA's specials 21, 22 and 23 (`17dc:1c9b`, `17dc:1a26`, `17dc:1b41`) and PARTY's special 1e
(`19b8:104e`), both on map 2 (area 0x8a: four areas north and two west of map 11). Map 3
(area 0x7d), with the jail and mode 9, comes after that. The towns by area: 0x28 map 9, 0x4b 8,
0x62 10, 0x66 13, 0x7d 3, 0x8a 2, 0xa6 7, 0xb9 4, 0xc2 5, 0xcc 1 (and 11), 0xd5 6
(`tables.area_maps`).

Reaching map 2 on foot: its south and east sides are walls and its north edge is blocked too
(on map 2 a step onto a tile from 0x55 is blocked, docs/movement.md). The only opening that leads
anywhere is the west side at rows 27-31 (cells 0-5), and it reaches 25 cells and one door: entrance
10, the second entrance of the 'Mech Garage (GARAGE, which sets flag 18). The rows 50-51 gap on the
same side is a pocket closed by the compound's wall. The rest of the town is reached from inside.

What map 2 offers Jason alone, inside (after the garage on foot, flag 18 set, 170 C-bills; every
door is reachable from the rear exit, 24 to 100 steps; `if_flag` jumps when the flag is not 0):

- HOSPITAL's recruit (menu item 3, 0623): flag 18 set goes to a refusal and back to the menu;
  with it clear, special 0e must find a member after Jason. Alone, never.
- REPAIR's recruit (0c1a): special 0e must find a member; alone it offers the Tech course (500).
- ARENA (03a0): entering costs 250 (`if_cbills 0xfa`); the prize (`earn 0x1f4`) and the 'Mech
  record specials 21-23 are all behind it.
- JAIL (map 3) is the recruit open to Jason alone: special 0e with a member goes to 01fb
  (special 25, end); alone it reaches the menu at 0277, whose item 07dc leads to `new_member 4`.
  So the story's first recruit is on map 3, and map 2's recruits and arena come after it.

## Not rebuilt yet

- **EB/EC** read F:d451 and d450, which nothing rebuilt sets yet (0 in a new game); the
  rebuild never jumps. UnBattletech names them "training complete" and a milestone.
- **E4** (sound) and **E9** (new member) do nothing but log.
- The specials other than 01-05, 0c, 0e, 11, 17 and 2b (in hex, as in the table above; chr's log
  prints them in decimal).
- Talk outdoors (`17dc:17b9`): one of eight rumours (DGROUP:4e5e) at random. No rebuilt place
  is outdoors; the rebuild makes the draw and logs it.
- The second barracks' follow-on (into VIEWDISK) needs flags that no rebuilt place sets yet. The
  garage on foot is rebuilt (special 11, then flag 18 and the party put inside at the rear exit);
  its 'Mech paths (specials 1c, 1d, 2e) are not.

## Other descriptions

The opcode set agrees with Elthial/CrescentHawksTools `BldDisassembler.cs` (MIT) in every operand
size. velteyn/UnBattletech `docs/formats/bld-bytecode.md` (GPL-3) agrees on most meanings, with
these differences, all checked against the program:
- **E6/E7.** They call these cursor X/Y. They are the party's map place (a44b/a44d, the same words
  the walk code uses, docs/movement.md).
- **FD/FE.** They call these no-ops. They are clear-window and select-window.
- **E4.** They call it write-char. It plays a sound.
- **The header.** Their 8-byte header is covered in formats/bld.md.
