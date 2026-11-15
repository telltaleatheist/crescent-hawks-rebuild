#include "formats/icn.h"

#include <string>

#include "formats/cmp.h"

namespace chr::formats {

// BTECH.EXE 1000:4621 (Ghidra FUN_1000_4621), for BTTLTECH.ICN
// BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8), for DESTRUCT.ICN
// Tile sheets go through the same loader and unpacker as the .CMP pictures
// (273d:063b, 273d:049d); the shipped sheets are all in row order.
TileSheet decode_icn(std::span<const std::uint8_t> file) {
    return TileSheet{decode_cmp(file).packed};
}

// BTECH.EXE 287f:1b1a (Ghidra FUN_287f_1b1a)
// The map drawer finds tile n at n * 128 in the unpacked sheet and copies 16
// rows of 8 bytes; 287f:1af4 computes the offset from the cell's byte.
std::array<std::uint8_t, kTileSize * kTileSize> TileSheet::tile(int index) const {
    if (index < 0 || index >= kTilesPerSheet)
        throw FormatError("ICN: no tile " + std::to_string(index));
    std::array<std::uint8_t, kTileSize * kTileSize> out{};
    const std::size_t base = std::size_t(index) * kTileBytes;
    for (int i = 0; i < kTileBytes; ++i) {
        out[2 * i] = packed[base + i] >> 4;
        out[2 * i + 1] = packed[base + i] & 0x0f;
    }
    return out;
}

}  // namespace chr::formats
