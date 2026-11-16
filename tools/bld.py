#!/usr/bin/env python3
"""List a place script (.BLD) from the player's own copy of the game.

    tools/bld.py ORIGINAL_DIR/CITADEL.BLD          the reachable code, with text IDs
    tools/bld.py --raw ORIGINAL_DIR/CITADEL.BLD    the deciphered bytes to stdout

A reading aid for docs/scripts.md: it deciphers the file (docs/formats/bld.md)
and walks the script from offset 0, as src/formats/bld.cpp does. Nothing it
prints goes in the repo (docs/rules.md, rule 1).
"""
import argparse
import os
import struct
import sys

# Menu item counts, BTECH.EXE 385b:0092 (src/formats/menus.h).
MENU_ITEMS = [20, 8, 0, 11, 5, 1, 2, 3, 5, 4, 3, 3, 4, 6, 5, 4, 7, 4, 3, 3, 3,
              6, 2, 7, 2, 5, 8, 6, 7, 2, 3, 2, 3, 6, 6, 2, 2, 20, 3, 8, 7]

# opcode: (name, operands) with b byte, w word, s text, M menu table, T switch table
OPS = {
    0xE4: ("sound", "b"), 0xE5: ("earn", "w"), 0xE6: ("set_party_place", "ww"),
    0xE7: ("if_party_x", "ww"), 0xE8: ("if_random", "bw"), 0xE9: ("new_member", "b"),
    0xEA: ("picture", "bb"), 0xEB: ("if_d451", "w"), 0xEC: ("if_d450", "w"),
    0xED: ("if_skill", "bbw"), 0xEE: ("pay", "w"), 0xEF: ("if_cbills", "ww"),
    0xF0: ("cursor", "bb"), 0xF1: ("add_flag", "bb"), 0xF2: ("pause", ""),
    0xF3: ("switch_flag", "bT"), 0xF4: ("set_flag", "bb"), 0xF5: ("special", "b"),
    0xF6: ("if_yes", "w"), 0xF7: ("if_flag", "bw"), 0xF8: ("goto", "w"),
    0xF9: ("menu", "bM"), 0xFA: ("frame", "b"), 0xFB: ("wait_key", ""),
    0xFC: ("print", "s"), 0xFD: ("clear", ""), 0xFE: ("window", "b"), 0xFF: ("end", ""),
}
JUMPS = {"if_party_x", "if_random", "if_d451", "if_d450", "if_skill", "if_cbills",
         "if_yes", "if_flag", "goto"}


def decipher(data):
    (length,) = struct.unpack_from("<H", data, 0)
    return bytes(((b + 0x29) & 0xFF) ^ 0xE9 for b in data[2:2 + length])


def decode(code, at):
    op = code[at]
    if op not in OPS:
        return {"at": at, "name": None, "raw": op, "next": at + 1, "targets": [], "falls": True}
    name, layout = OPS[op]
    p, args, targets, text = at + 1, [], [], None
    for k in layout:
        if k == "b":
            args.append(code[p]); p += 1
        elif k == "w":
            args.append(struct.unpack_from("<H", code, p)[0]); p += 2
        elif k == "s":
            end = code.index(0, p); text = code[p:end]; p = end + 1
        elif k == "M":
            for _ in range(MENU_ITEMS[args[0]]):
                targets.append(struct.unpack_from("<H", code, p)[0]); p += 2
        elif k == "T":
            end = None
            while end is None or p < end:
                t = struct.unpack_from("<H", code, p)[0]; p += 2
                targets.append(t)
                if t >= p and (end is None or t < end):
                    end = t
    if name in JUMPS:
        targets.append(args[-1])
    return {"at": at, "name": name, "raw": op, "args": args, "text": text, "next": p,
            "targets": targets, "falls": name not in ("end", "goto", "menu", "switch_flag")}


def walk(code):
    seen, todo = {}, [0]
    while todo:
        at = todo.pop()
        while at < len(code) and at not in seen:
            ins = decode(code, at)
            seen[at] = ins
            todo += [t for t in ins["targets"] if t < len(code)]
            if not ins["falls"]:
                break
            at = ins["next"]
    return seen


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--raw", action="store_true", help="write the deciphered script to stdout")
    a = ap.parse_args()
    code = decipher(open(a.file, "rb").read())
    if a.raw:
        sys.stdout.buffer.write(code)
        return
    stem = os.path.splitext(os.path.basename(a.file))[0].lower()
    labels = {t for ins in walk(code).values() for t in ins["targets"]}
    for at, ins in sorted(walk(code).items()):
        mark = "L" if at in labels else " "
        if ins["name"] is None:
            print(f"{mark}{at:04x}  .byte {ins['raw']:02x}")
            continue
        args = " ".join(f"{x:#x}" for x in ins["args"])
        if ins["name"] == "print":
            args = f"bld.{stem}.{at:04x}  {ins['text'][:60]!r}"
        if ins["name"] in ("menu", "switch_flag"):
            args += "  -> " + " ".join(f"{t:04x}" for t in ins["targets"])
        print(f"{mark}{at:04x}  {ins['name']:<15} {args}")


if __name__ == "__main__":
    main()
