// The land around the party: the nine areas of the world nearest it, each
// a grid of 8x8 blocks (docs/wilderness.md). A block is either one of the
// loaded map's own or a terrain block, a terrain type the program makes up
// from the world's heights. No pixels here: the window hands out tile
// numbers.
#pragma once

#include <array>
#include <cstdint>

#include "formats/mtp.h"
#include "game/fixes.h"

namespace chr::formats {
struct Tables;
}

namespace chr::game {

class Terrain {
public:
    static constexpr int kGrids = 9;         // 3x3 areas, the party's in the middle (grid 4)
    static constexpr int kGridBlocks = 64;   // 8x8 blocks an area
    static constexpr int kFirstMapBlock = 0x90;  // a grid entry naming the map's block 0

    Terrain(const formats::Tables& tables, const Fixes& fixes);

    // BTECH.EXE 287f:104e (Ghidra FUN_287f_104e)
    // The nine grids around `area` (0xYX), each made from the heights of
    // its area's four corners, then their neighbour masks (287f:1886).
    void generate(int area);

    // BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8), the part that places the
    // map: its blocks go into grid `grid` at the block column and row the
    // map's header gives, numbered from kFirstMapBlock. The rest of the
    // grid keeps its terrain.
    void place_map(const formats::MtpMap& map, int grid);

    // BTECH.EXE 287f:1da8 (Ghidra FUN_287f_1da8), 287f:1886 and 287f:11bb
    // Each block's neighbour mask: which of its four neighbours have the
    // same terrain type. A map block's mask is its number in the block store.
    void link_blocks();

    // P:0564: the grids, 64 entries each, row by row.
    const std::array<std::uint8_t, kGrids * kGridBlocks>& grids() const { return grids_; }
    // P:0324: the masks, in the same order.
    const std::array<std::uint8_t, kGrids * kGridBlocks>& masks() const { return masks_; }

    // BTECH.EXE 1000:50c8 (Ghidra FUN_1000_50c8), its first part
    // P:09fb: the 256 random numbers start-up draws first, from the
    // program image's random state. The generator takes its noise from them.
    static std::array<std::uint8_t, 256> start_up_noise();

private:
    // The height the generator reads at `index` from the world table's
    // start (docs/wilderness.md: it reads past the table at the world's edges).
    int height(int index) const;
    // BTECH.EXE 287f:0bfb (Ghidra FUN_287f_0bfb)
    // One area's 8x8 terrain types from its four corner heights.
    std::array<std::uint8_t, kGridBlocks> area_types(int nw, int ne, int sw, int se) const;

    const formats::Tables& tables_;
    const Fixes& fixes_;
    std::array<std::uint8_t, 256> noise_{};
    std::array<std::uint8_t, kGrids * kGridBlocks> grids_{};
    std::array<std::uint8_t, kGrids * kGridBlocks> masks_{};
};

}  // namespace chr::game
