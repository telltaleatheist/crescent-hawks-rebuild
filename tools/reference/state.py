#!/usr/bin/env python3
"""The game state the rig compares, read from the original's memory.

    state.py CAPTURE.bin

Prints the same lines as `chr --state` (random numbers, C-Bills, the
townspeople, the party's place, the script flags, Jason's skills and the
menus' remembered items), from a capture with memory (tools/capture/snap.asm). The
addresses are docs/townspeople.md's.
"""
import struct
import sys

from memory import Memory


def original_state(path):
    m = Memory(path)
    far = m.word(m.ds, 0x538a)        # the segment of DGROUP:d3xx
    xs = m.word(m.ds, 0x53a4)         # figure x positions (4004)
    ys = m.word(m.ds, 0x53a6)         # figure y positions (4036)
    heads = m.word(m.ds, 0x5578)      # figure headings (3920)
    r = m.read(m.ds, 0x4fc0, 3)
    lines = ["state random %02x %02x %02x" % tuple(r),
             "state cbills %d" % struct.unpack("<i", m.read(far, 0xd370, 4))[0]]
    for i in range(8):
        rec = 0xd390 + 0x1a * i
        heading = struct.unpack("b", m.read(heads, 0x3920 + 0x10 + i, 1))[0]
        lines.append("state walker %d at %04x %04x target %04x %04x route %02x wait %02x heading %d" % (
            i, m.word(xs, 0x4004 + 2 * (0x10 + i)), m.word(ys, 0x4036 + 2 * (0x10 + i)),
            m.word(far, rec + 4), m.word(far, rec + 6), m.byte(far, rec + 8), m.byte(far, rec + 9), heading))
    lines.append("state party %04x %04x" % (m.word(m.word(m.ds, 0x538c), 0xa44b), m.word(m.word(m.ds, 0x538e), 0xa44d)))
    lines.append("state flags " + m.read(far, 0xd30c, 0x64).hex())          # docs/scripts.md
    lines.append("state skills " + m.read(far, 0xc614 + 4, 7).hex())       # Jason's record, bytes 4-10
    menus = m.word(m.ds, 0x56de)                                            # the menu table, 385b:0092
    lines.append("state menus " + " ".join("%d" % m.word(menus, 0x98 + 16 * i) for i in range(41)))
    return lines


if __name__ == "__main__":
    print("\n".join(original_state(sys.argv[1])))
