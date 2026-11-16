# .ANM: the places' pictures

`O<n>.ANM` is the picture in the upper left box when the party is in a place (docs/scripts.md,
"The picture"). It is an 88×88 picture in 16 colours, stored as frames of changes. Read by
`src/formats/anm.cpp`; the default asset provider serves it as `{"O<n>.ANM", frame}`, with frame
0xffff meaning the last one (what stays on screen).

## Layout

    +00  frame list: one byte per frame shown; the list ends at the first 0 after entry 0
    +32  speed (signed byte)
    +33  the frames' data, one after the other

The program loads the file at P:42c3 and keeps the delays just before it, at DGROUP:42a2, so
a list entry s names the delay byte at file offset s − 0x21 (signed). Frame k stays up for
delay · speed · 3 / 4 ticks (`1000:1afd`). The first frame always shows, whatever entry 0 holds.

## A frame (BTECH.EXE 287f:23ec)

Each frame is XORed onto the one before, starting from all zeros. The picture is 3872 bytes, two
pixels a byte, the left pixel in the high nibble, row by row. A frame's data is runs:

| bytes | meaning |
|-------|---------|
| n, then n bytes (n 01–7f) | the bytes as they are |
| n, v (n 80–ff) | v, 256 − n times |
| 00, hh, ll, v | v, hh·256 + ll times (big-endian; a count of 0 means 65536, the program's 16-bit loop) |

A frame ends once 3872 bytes are done, even inside a run; the next frame starts after that run.

## Showing it

The pixels are colour numbers in the game's own EGA palette, the same as the tiles and sprites,
drawn at screen (8, 8) after conversion to EGA planes (`287f:0572`, `287f:1e37`).
`287f:00d1` maps colours for CGA only and doesn't apply. Checked against the original: the
rebuild's Citadel picture (O17) matches the original's screen pixel for pixel (rig case
`citadel_inside`).
