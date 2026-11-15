#!/usr/bin/env python3
"""Unpack a DOS MZ executable compressed with Microsoft EXEPACK.

Usage: unexepack.py IN.EXE OUT.EXE

EXEPACK keeps a small header at the packed entry point (CS:0000, entry IP = 0x10
or 0x12). Fields: real IP, real CS, mem_start, exepack_size, real SP, real SS,
dest_len (paragraphs), [skip_len], signature "RB". The image before CS is
decompressed backwards from its end; the relocation table (16 segment groups of
count + offsets) follows the "Packed file is corrupt" message in the stub.
"""
import struct
import sys


def unpack(data: bytes) -> bytes:
    if data[:2] != b"MZ":
        raise SystemExit("not an MZ executable")
    (last, pages, nrel, hdr_par, min_al, max_al, ss, sp, _csum, ip, cs,
     _relo, _ovl) = struct.unpack("<13H", data[2:28])
    if nrel:
        raise SystemExit("has relocations: not EXEPACKed")
    size = (pages - 1) * 512 + last if last else pages * 512
    hdr = hdr_par * 16
    image = bytearray(data[hdr:size])

    blk = cs * 16
    if ip == 0x10:
        f = struct.unpack("<7H", image[blk:blk + 14])
        sig = image[blk + 14:blk + 16]
        skip = 1
    elif ip == 0x12:
        f = struct.unpack("<8H", image[blk:blk + 16])
        sig = image[blk + 16:blk + 18]
        skip = f[7]
    else:
        raise SystemExit(f"unexpected entry IP {ip:#x}")
    if sig != b"RB":
        raise SystemExit("EXEPACK signature RB not found")
    real_ip, real_cs, _mem, exepack_size, real_sp, real_ss, dest_len = f[:7]

    # Decompress in place, backwards.
    buf = image[:blk] + bytearray(max(0, dest_len * 16 - blk))
    src = blk - (skip - 1) * 16
    while src > 0 and buf[src - 1] == 0xFF:
        src -= 1
    dst = dest_len * 16
    while True:
        cmd = buf[src - 1]
        length = buf[src - 3] | (buf[src - 2] << 8)
        src -= 3
        op = cmd & 0xFE
        if op == 0xB0:
            fill = buf[src - 1]
            src -= 1
            buf[dst - length:dst] = bytes([fill]) * length
            dst -= length
        elif op == 0xB2:
            buf[dst - length:dst] = buf[src - length:src]
            dst -= length
            src -= length
        else:
            raise SystemExit(f"bad EXEPACK command {cmd:#x} at {src:#x}")
        if cmd & 1:
            break
    out_image = bytes(buf[:dest_len * 16])

    # Relocations follow the error message in the stub.
    stub = bytes(image[blk:blk + exepack_size])
    msg = b"Packed file is corrupt"
    k = stub.find(msg)
    if k < 0:
        raise SystemExit("relocation table not found")
    p = k + len(msg)
    relocs = []
    for seg in range(16):
        (count,) = struct.unpack_from("<H", stub, p)
        p += 2
        for _ in range(count):
            (off,) = struct.unpack_from("<H", stub, p)
            p += 2
            relocs.append((off, seg * 0x1000))

    # Build a plain MZ.
    rel_off = 0x1C
    hdr_len = rel_off + 4 * len(relocs)
    hdr_len = (hdr_len + 511) // 512 * 512
    total = hdr_len + len(out_image)
    extra = (dest_len * 16 - len(out_image))  # none, kept for clarity
    # min alloc: the packed file's need, measured from the end of the new image.
    need_par = (size - hdr) // 16 + min_al - dest_len
    stack_par = (real_ss * 16 + real_sp + 15) // 16 - dest_len
    new_min = max(0, need_par + extra // 16, stack_par)
    header = struct.pack(
        "<2s13H", b"MZ", total % 512, (total + 511) // 512, len(relocs),
        hdr_len // 16, min(new_min, 0xFFFF), max_al, real_ss, real_sp, 0,
        real_ip, real_cs, rel_off, 0)
    header += b"".join(struct.pack("<HH", o, s) for o, s in relocs)
    header = header.ljust(hdr_len, b"\0")
    return header + out_image


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], "rb").read()
    out = unpack(data)
    open(sys.argv[2], "wb").write(out)
    nrel = struct.unpack_from("<H", out, 6)[0]
    print(f"unpacked {len(data)} -> {len(out)} bytes, {nrel} relocations")


if __name__ == "__main__":
    main()
