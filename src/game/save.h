// A saved game's file, GameN (docs/formats/save.md).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace chr::game::save {

inline constexpr std::uint8_t kMark = 0x0c;  // the file's first byte (1000:368b)
inline constexpr int kBlockStart = 0xc614;   // F:c614, the block's first byte
inline constexpr int kBlockSize = 0xf44;     // to F:d557
inline constexpr int kFileSize = 1 + kBlockSize + 2 + 2;  // the mark, the block, x, y
inline constexpr int kSlots = 6;             // menu 0x28's items 0-5; item 6 cancels

using Block = std::array<std::uint8_t, kBlockSize>;

// Where the byte at F:address sits in the block.
constexpr std::size_t at(int address) { return std::size_t(address - kBlockStart); }

// The parts of the block the rebuild holds. Every other byte is kept as the
// last load read it (or 0, as the program image has it) and written back
// unchanged (Town::save_shadow_).
struct Part {
    int address;  // F:
    int size;
    std::string_view what;
};
inline constexpr std::array kHeld = {
    Part{0xc614, 16 * 0x11, "the character records"},
    Part{0xc724, 8 * 0x7d, "the 'Mech records"},
    Part{0xcb0c, 0x800, "the seen map"},
    Part{0xd30c, 0x64, "the script flags"},
    Part{0xd370, 4, "C-Bills"},
    Part{0xd374, 3 * 4, "the investments"},
    // The townspeople's records, F:d390 + 0x1a i: +0/+2 where kept for a
    // fight, +4/+6 the target, +8 the route, +9 the wait.
    Part{0xd390, 8 * 0x1a, "the townspeople's records, bytes 0-9"},
    Part{0xd450, 2, "the care's kits"},
    Part{0xd452, 4, "the 'Mechs held at the garage"},
    Part{0xd456, 1, "the next name number"},
    Part{0xd457, 4 * 64, "the map marks"},
    Part{0xd557, 1, "the marks' next slot"},
};

// "Game1" to "Game6" (DG:0154, the digit at DG:0158 = '1' + slot).
std::string file_name(int slot);

}  // namespace chr::game::save
