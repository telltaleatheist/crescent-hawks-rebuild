#!/usr/bin/env python3
"""Read the original's memory from a capture (tools/capture/snap.asm).

    memory.py CAPTURE.bin SPEC...

A SPEC is SEG:OFF[:LEN] in hex, where SEG may be `ds` (the game's data
segment when it last asked for a key) or `[OFF]` (the segment whose number
is the word at DS:OFF, the way the game reaches its far data, e.g. [538a]).
Prints the bytes in hex. As a module: Memory(path).read(seg, off, n).
"""
import struct
import sys

PICTURE_START = 24 + 768
EGA_PICTURE = 32000


class Memory:
    def __init__(self, path):
        data = open(path, "rb").read()
        at = PICTURE_START + EGA_PICTURE
        if data[at:at + 4] != b"MEMS":
            raise ValueError(f"{path}: no memory in this capture")
        self.ds = struct.unpack_from("<H", data, at + 4)[0]
        self.mem = data[at + 6:]

    def segment(self, spec):
        if spec == "ds":
            return self.ds
        if spec.startswith("["):
            return self.word(self.ds, int(spec.strip("[]"), 16))
        return int(spec, 16)

    def read(self, seg, off, n=1):
        base = seg * 16 + off
        return self.mem[base:base + n]

    def byte(self, seg, off):
        return self.mem[seg * 16 + off]

    def word(self, seg, off):
        return struct.unpack_from("<H", self.mem, seg * 16 + off)[0]


def main():
    m = Memory(sys.argv[1])
    for spec in sys.argv[2:]:
        parts = spec.split(":")
        seg = m.segment(parts[0])
        off = int(parts[1], 16)
        n = int(parts[2], 16) if len(parts) > 2 else 1
        print(f"{seg:04x}:{off:04x}", m.read(seg, off, n).hex(" "))


if __name__ == "__main__":
    main()
