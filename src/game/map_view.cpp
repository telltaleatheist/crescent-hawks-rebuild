#include "game/map_view.h"

#include <algorithm>

namespace chr::game {

// BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8) loads DESTRUCT.ICN for map 11;
// BTECH.EXE 1b5d:0004 (Ghidra FUN_1b5d_0004) loads STARLEAG.ICN with map 14;
// BTECH.EXE 1000:4621 (Ghidra FUN_1000_4621) restores BTTLTECH.ICN otherwise.
std::string tile_sheet_for_map(int map_number) {
    if (map_number == 11) return "DESTRUCT.ICN";
    if (map_number == 14) return "STARLEAG.ICN";
    return "BTTLTECH.ICN";
}

// BTECH.EXE 1000:4dc7 (Ghidra FUN_1000_4dc7)
// New-game setup: loads map 1 into the middle area and puts the party at
// x 0x0c45, y 0xc019 (half-cell 69, 25 of that area). The x is the Citadel's
// entrance (entry 1 in MAP1's table); the y is five half-cells below it.
formats::MapPosition new_game_position() { return {0x0c45, 0xc019}; }

// BTECH.EXE 287f:18ef (Ghidra FUN_287f_18ef)
// The map drawer shows 13 whole tiles across and 12 down, plus a half tile
// at one edge of each: the left (right half of the tile) when the x
// half-cell is odd, else the right (left half); the top (bottom half) when
// the y half-cell is odd, else the bottom (top half). Either way the view
// starts 12 half-cells before the position. 287f:1314 and 287f:13d9 first
// copy the 3x3 blocks around the position into a 24x24-cell window.
MapView view_of(const Terrain& terrain, const std::string& tile_sheet, formats::MapPosition position) {
    MapView view;
    view.origin_half_x = position.half_cell_x() - kViewLead;
    view.origin_half_y = position.half_cell_y() - kViewLead;

    auto floor_half = [](int h) { return h >= 0 ? h / 2 : -((1 - h) / 2); };
    const int first_x = floor_half(view.origin_half_x);
    const int last_x = floor_half(view.origin_half_x + kViewHalfCellsWide - 1);
    const int first_y = floor_half(view.origin_half_y);
    const int last_y = floor_half(view.origin_half_y + kViewHalfCellsHigh - 1);

    for (int y = first_y; y <= last_y; ++y)
        for (int x = first_x; x <= last_x; ++x)
            if (const auto tile = terrain.cell(position, x, y)) view.cells.push_back({x, y, AssetId{tile_sheet, std::uint16_t(*tile)}});
    return view;
}

int view_corner_index(formats::MapPosition party) {
    return (((party.half_cell_y() >> 1) & 7) + 2) * kWindowCells + ((party.half_cell_x() >> 1) & 7) + 2;
}

}  // namespace chr::game
