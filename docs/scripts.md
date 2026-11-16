# The place scripts

What happens inside a place: the interpreter for the .BLD scripts (the file is in
[formats/bld.md](formats/bld.md)). `tools/bld.py ORIGINAL/CITADEL.BLD` lists a script with these
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
   entered at all. Only a failed quiz sets flag 14, so the quiz comes back on every visit until
   then (docs/combat.md, "The copy-protection check").
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

The script prints the menu's words first, as ordinary text. The menu record (menus.h) says where
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
| 01–03 | 24d3:0027, 0187, 01c2 | TRAINING | the training missions (to be read in part B) |
| 04 | 24d3:0231 | CITADEL | prints the three courses' prices: for skill i (0–2), 125 × Jason's skill i + 75, right-aligned to column 39 on row 5 + i |
| 05 | 24d3:02a5 | CITADEL | takes the course in flag 08: prints DGROUP:491a and the level and course names (tables DGROUP:4ea2, 4eb6). At level 4 it prints 4948 and sets flag 09 = 0. With enough C-Bills: flag 09 = 1, the skill + 1, the price paid. Otherwise it prints 4999 and sets flag 09 = 0. |
| 12 | 24d3:0aea | ten places | "talk to others": `17dc:17b9`, below |
| 14 | 24d3:0af2 | ten places, and after every visit | flag 0e = how many of the party records 1–7 are in use |
| others | see the table above | the other places | read with those places |

Specials 6–11 and 13–47 belong to the other places and are read with them. The rebuild logs an
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

## Not rebuilt yet

- **EB/EC** read F:d451 and d450, which nothing rebuilt sets yet; the rebuild never jumps.
- **E4** (sound) and **E9** (new member) do nothing but log.
- The specials other than 4, 5, 12 and 14, and the training center's quiz (1c67:08a8; only its draws are rebuilt).

## Other descriptions

The opcode set agrees with Elthial/CrescentHawksTools `BldDisassembler.cs` (MIT) in every operand
size. velteyn/UnBattletech `docs/formats/bld-bytecode.md` (GPL-3) agrees on most meanings, with
these differences, all checked against the program:
- **E6/E7.** They call these cursor X/Y. They are the party's map place (a44b/a44d, the same words
  the walk code uses, docs/movement.md).
- **FD/FE.** They call these no-ops. They are clear-window and select-window.
- **E4.** They call it write-char. It plays a sound.
- **The header.** Their 8-byte header is covered in formats/bld.md.
