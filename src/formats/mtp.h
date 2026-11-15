// .MTP: the town and area maps (docs/formats/mtp.md).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace chr::formats {

// A position as the program stores it: two 16-bit words. The low 7 bits of
// each count half-cells (8 pixels) inside a 64x64-cell area; higher bits say
// which area of the world (x: bits 8-11, y: bits 12-15).
struct MapPosition {
    std::uint16_t x = 0;
    std::uint16_t y = 0;

    int half_cell_x() const { return x & 0x7f; }
    int half_cell_y() const { return y & 0x7f; }
    bool operator==(const MapPosition&) const = default;
};

inline constexpr int kBlockCells = 8;  // a block is 8x8 cells, 64 bytes

struct MtpMap {
    std::uint8_t unknown_first_byte = 0;  // never read by the loader
    int block_x = 0, block_y = 0;         // where in the area's 8x8 block grid it goes
    int width = 0, height = 0;            // in cells, multiples of 8

    std::array<std::string, 8> people;      // 16-byte names
    std::array<std::string, 16> places;     // 16-byte names, one per entry in `entrances`
    std::array<MapPosition, 16> entrances;  // walking onto one offers to enter that place
    std::array<MapPosition, 16> second_positions;  // a second table in the same form (use not known)
    std::array<std::uint8_t, 16> unknown_16{};     // not yet traced
    std::array<std::uint8_t, 8> start_shuffle{};   // picks people's starting places (1000:2da8)

    std::vector<std::uint8_t> blocks;  // width*height bytes, block after block

    // The tile number at cell (x, y) of this map.
    std::uint8_t cell(int x, int y) const;
};

MtpMap decode_mtp(std::span<const std::uint8_t> file);

}  // namespace chr::formats
