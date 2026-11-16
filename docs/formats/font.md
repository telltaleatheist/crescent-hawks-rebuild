# The font, and other data read from BTECH.EXE

## Where it lives

The game's 8x8 font is not a file: it is 1,024 bytes inside BTECH.EXE, at 2c6c:a661 in the
unpacked program (image offset 0x26d21; file offset 0x2a121 in the unpacked file, whose header is
0x3400 bytes). 128 characters, 8 bytes each, one byte per row, the top bit the leftmost pixel.
`273d:00d5` masks every character with 0x7f before looking it up. Characters 2 and 6 are
pictures (an arrow and a check mark) but inside printed text those bytes are colour codes
(docs/text.md).

The EGA driver draws a character with `287f:2251`; the CGA and Tandy drivers draw with
`287f:2209` and `287f:21a8` from a widened copy made by `287f:2cf7`. All of those are display
drivers, replaced by the platform layer.

## How the rebuild reads it

`src/formats/exe_data.{h,cpp}`: the player's BTECH.EXE is unpacked in memory
(`src/formats/exepack.cpp`, the same steps as `tools/unexepack.py`), and the result's SHA-256 is
checked against the DOS 1.03 program (056b4067...). Only then is a named piece handed back
("font", the texts in docs/text.md, the names). Any other program fails with a message naming
the version we support; the rebuild never searches a program for something that looks like a
font. Another version can be added with its own table when someone has one.

The asset provider answers `{"FONT", c}` with character c in white on transparent; the
renderer colours it. A makeover replaces those IDs with its own font.

## Frame pieces: BTBORDER.CMP

At start-up (`1527:0044`) the program unpacks BTBORDER.CMP and cuts 8 cells of 8x8 pixels from it
(`2656:0ae5`), left to right from the top-left corner (a row of cells is 40 across). Those are
the frame pieces 0-7 of docs/text.md. The provider answers `{"BTBORDER.CMP:8x8", n}` with cell n.
TINYLAND.CMP (0x42 cells) and MECHSHAP.CMP are cut the same way, for later.
