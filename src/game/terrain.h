// The land around the party: the nine areas of the world nearest it, each
// a grid of 8x8 blocks (docs/wilderness.md). A block is either one of the
// loaded map's own or a terrain block, a terrain type the program makes up
// from the world's heights. No pixels here: the window hands out tile
// numbers.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

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

    // The cell window (P:07ad) and what it holds (P:07a4).
    static constexpr int kWindowCells = 24;
    struct Window {
        std::array<std::uint8_t, kWindowCells * kWindowCells> cells{};  // tile numbers, row by row
        std::array<std::uint8_t, 9> blocks{};  // each block's mask OR grid entry, row by row
    };
    // BTECH.EXE 287f:1314 (Ghidra FUN_287f_1314)
    // The 3x3 blocks around the block holding `position`, from the grids:
    // map blocks from `map`, terrain blocks from the templates.
    Window window(formats::MapPosition position, const formats::MtpMap& map) const;
    // The window around `position` with the map last placed, built once
    // for each block the position is in.
    const Window& window(formats::MapPosition position) const;
    // The placed map's cells have changed: the window is built again.
    void map_changed() { built_.reset(); }
    // A map load (1000:2da8) leaves the cell window as it was until 287f:1314
    // next builds one: the window as last built is held for what reads the
    // original's memory (the rig), until release_window().
    void hold_window() {
        if (!built_) return;
        held_ = built_;
        held_column_ = built_column_;
        held_row_ = built_row_;
    }
    // BTECH.EXE 287f:1ece (Ghidra FUN_287f_1ece)
    // The fight saves the window's cells after a round's movement
    // (22e8:126a, around the party) and each redraw during a shot
    // (1e31:1f09) copies them back, leaving the window's place and block
    // values as they were.
    void save_cells(formats::MapPosition position) const { saved_ = window(position).cells; }
    void restore_cells() const {
        if (saved_) restored_ = saved_;
    }
    // The round's map drawing (287f:1314) builds the window again.
    void release_window() const {
        held_.reset();
        restored_.reset();
    }
    // The window the original's memory holds for `position`'s block.
    Window held_window(formats::MapPosition position) const {
        const int column = (position.x & 0xff) >> 4, row = (position.y & 0xff) >> 4;
        if (held_ && column == held_column_ && row == held_row_) return *held_;
        if (restored_) {
            static const formats::MtpMap kNoMap{};
            Window w = window(position, map_ ? *map_ : kNoMap);
            w.cells = *restored_;
            return w;
        }
        return window(position);
    }
    // Cell `index` of that window. Before it, as in the original's memory,
    // the nine block values (P:07a4), the grids (P:0564) and the masks
    // (P:0324); nothing past it or before those (memory the rebuild does
    // not hold).
    std::optional<int> window_cell(formats::MapPosition position, int index) const;
    // The cell (x, y) of the position's area (either may be outside 0..63:
    // a neighbouring area's) if the window around `position` holds it.
    std::optional<int> cell(formats::MapPosition position, int x, int y) const;

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
    // BTECH.EXE 287f:13d9 (Ghidra FUN_287f_13d9)
    // Block (column, row) of grid 4, -1..8 (one past its edge is the next
    // grid's), into `out` (stride kWindowCells); returns its mask OR entry.
    std::uint8_t copy_block(int column, int row, const formats::MtpMap& map, std::uint8_t* out) const;
    // BTECH.EXE 287f:0bfb (Ghidra FUN_287f_0bfb)
    // One area's 8x8 terrain types from its four corner heights.
    std::array<std::uint8_t, kGridBlocks> area_types(int nw, int ne, int sw, int se) const;

    const formats::Tables& tables_;
    const Fixes& fixes_;
    std::array<std::uint8_t, 256> noise_{};
    std::array<std::uint8_t, kGrids * kGridBlocks> grids_{};
    std::array<std::uint8_t, kGrids * kGridBlocks> masks_{};
    const formats::MtpMap* map_ = nullptr;
    mutable std::optional<Window> built_;
    mutable int built_column_ = -1, built_row_ = -1;
    mutable std::optional<Window> held_;
    int held_column_ = -1, held_row_ = -1;
    mutable std::optional<std::array<std::uint8_t, kWindowCells * kWindowCells>> saved_, restored_;
};

}  // namespace chr::game
