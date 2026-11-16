# Text on the screen

The program writes all its text and box frames into a 40x25 grid of 8x8-pixel cells (the
320x200 screen). Rebuilt in `src/game/text.{h,cpp}`; drawn by `src/platform/text_layer.cpp`.
The game logic only says which glyph or frame piece goes in a cell, in which colours; the
renderer looks each one up by asset ID (`{"FONT", c}`, `{"BTBORDER.CMP:8x8", n}`), so a
makeover can replace the font and the frames.

## Text windows

A table of 16-byte records at 385b:0000 describes the windows: x, y, width,
height (in cells), foreground and background colour, and the cursor's column and row.
`2656:0281` selects a window (keeping the old one's colours and cursor), `2656:0388` clears it.

| # | x | y | w | h | colours | used for |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 40 | 25 | 15 on 0 | whole screen |
| 1 | 14 | 15 | 25 | 9 | 15 on 0 | |
| 2 | 1 | 1 | 38 | 23 | 15 on 0 | |
| 3 | 1 | 13 | 11 | 11 | 15 on 0 | panel, party box |
| 4 | 1 | 1 | 11 | 11 | 15 on 0 | panel, upper box |
| 5 | 10 | 1 | 29 | 16 | 15 on 0 | |
| 6 | 14 | 1 | 25 | 23 | 15 on 0 | new-game text |
| 7 | 14 | 10 | 25 | 5 | 15 on 0 | message box over the map |
| 8 | 0 | 0 | 16 | 25 | 15 on 0 | |
| 9 | 1 | 2 | 38 | 20 | 0 on 15, column 14 | |
| 10 | 0 | 0 | 25 | 8 | 0 on 15, column 14 | |

## Frames (2656:0004)

A frame goes one cell outside the current window. Each style is a list of frame pieces: the
four corners (top-left, top-right, bottom-left, bottom-right), then the top, left, right and
bottom edges, each a run of pieces ended by 0xff that repeats along the edge (`2656:01e7`).
The pieces are the 8 cells the start-up cuts from the top-left of BTBORDER.CMP (docs/formats/font.md).

| Style | Corners | Top | Left | Right | Bottom |
|---|---|---|---|---|---|
| 0, 1, 2 | 0 1 6 7 | 4 | 5 | 5 | 4 |
| 3 | 2 3 6 7 | 4 | 5 | 5 | 4 |
| 4 | 0 1 2 3 | 4 | 5 | 5 | 4 |

The panel (`1000:4cac`) frames window 3 with style 3 and window 4 with style 4, so the two boxes
share the middle bar: 13 cells, 104 pixels wide, beside the 216-pixel map.

## The printer (2656:03f5)

Prints a piece of text into the current window, word-wrapped, a word at a time. The text uses
these bytes:

| Byte | Meaning |
|---|---|
| top bit set | the last character (the rest of the byte is the character) |
| 0x00 | end |
| 0x0d | new line |
| 0x02 n | background colour n |
| 0x06 n | foreground colour n |
| 0x09 n | cursor to column n (column 0 if n is past the width) |
| 0x13 n | spaces up to column n, counted from the cursor |

A word that does not fit goes on the next line, leading spaces dropped; a word as wide as the
window is broken. Each line goes out through `2656:07cb`: below the last row it scrolls the
window up a row, clears the bottom row to colour 0 and writes there.

`273d:00d5` writes a string at a cell position without wrapping (the panel's names).

## The yes/no question (1000:1a13)

Prints a new line and then "Yes No" with the picked answer in black on yellow (colour 0 on 14).
Y or N answer at once; left picks Yes, right picks No; space or Enter accept the pick. After
every key the line is printed again over itself. The window's foreground colour is put back at
the end.

## Messages

`24d3:17c6` opens the message box: window 7, cleared, frame style 0.

## Text IDs

Every text the game shows has an ID and comes from the text provider (`src/game/text_provider.h`),
never from a literal in the code. The importer (docs/import.md) writes the program's texts with
their IDs; an override file can replace any of them.

| ID | Address | Text |
|---|---|---|
| walk.too_deep | 46db:0410 | The water is too deep that way. |
| walk.enter_question | 46db:0430 | Will you enter the (new line) |
| walk.enter_question_end | 46db:0444 | ? |
| walk.direction.north ... northwest | 46db:006c - 00a7 | (new line) North, Northeast, East, Southeast, South, Southwest, West, Northwest |
| ui.answer_yes_picked | 46db:03ee | the yes/no line, Yes picked |
| ui.answer_no_picked | 46db:03fe | the yes/no line, No picked |
| panel.party_heading | 46db:0aca | BDC (over the bar columns 9-11) |
| character.0 - character.7 | 46db:00b2 - 00d9 | the eight characters' names, Jason first |
| mapN.place.E | MAPN.MTP | the place behind entrance E |
| mapN.person.P | MAPN.MTP | the map's people |

The combat texts (import version 7) are grouped by the function that prints them; the import
table in `src/formats/exe_data.cpp` gives each one's address:

| IDs | Printed by | What |
|---|---|---|
| mech.&lt;id&gt;, weapon.0 - weapon.32 | (the rule tables) | the 'Mech templates' and weapons' names, read from inside the tables |
| fight.* | 203b:000a | the encounter, the engage question, the double agent's story (fight.agent.0-16), the end of a fight, the five encounter descriptions (fight.encounter.0-4) |
| orders.*, options.*, scan.* | 203b:14c3 and its helpers, 15ab:1467 | the orders menus, movement messages, the movement modes (orders.mode.0-2), the combat options, the Scan browser |
| weapons.*, pick.*, destroyed.* | 1d43 | the Use Weapons screen, the range bands (weapons.range.0-3), the target picker, a 'Mech destroyed |
| shot.* | 22e8:000c, 22e8:12c7, 1e31 | the pieces of a shot's message, hit and miss, critical, kills |
| location.0 - location.10 | 1e31:1b8f | the hit locations, by location index (armour byte 0x11 + index), not the program's name order |
| condition.0 - condition.10 | 1e31:031e | a person's condition, by hit points |
| armour.0 - armour.5 | (DG:4e8a) | the personal armour types |
| unit.*, after.* | 15ab | the unit panel; repairs, scrap, salvage and loot after a fight |
| mission.* | 17dc:0629 | the training missions' messages |

The question "Will you enter the Citadel?" is `walk.enter_question` + `map1.place.1` +
`walk.enter_question_end`. The yes/no keys are still Y and N whatever the language (to be made
data with the input layer).
