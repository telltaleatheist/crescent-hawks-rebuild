// What part of a map the player sees, in the original's units: map cells and
// half-cells (8 pixels on the original screen). No pixels here: the renderer
// turns each visible cell's tile ID into a picture.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "formats/mtp.h"
#include "game/asset_id.h"
#include "game/terrain.h"

namespace chr::game {

// The view is 27 half-cells wide and 25 high (216x200 on the original
// screen), with the position 12 half-cells in from its top-left corner.
inline constexpr int kViewHalfCellsWide = 27;
inline constexpr int kViewHalfCellsHigh = 25;
inline constexpr int kViewLead = 12;

struct VisibleCell {
    int cell_x = 0, cell_y = 0;  // map cell
    AssetId tile;                // the tile drawn there
};

struct MapView {
    int origin_half_x = 0;  // the half-cell at the view's top-left corner
    int origin_half_y = 0;
    std::vector<VisibleCell> cells;  // every map cell the view touches, even in part
};

// The tile sheet a map's cells index (1000:2da8, 1000:4621, 1b5d:0004).
std::string tile_sheet_for_map(int map_number);

// The position a new game starts at, on map 1 (1000:4dc7).
formats::MapPosition new_game_position();

// The cells come from the cell window (287f:1314), so past the map's edge
// they are the terrain's.
MapView view_of(const Terrain& terrain, const std::string& tile_sheet, formats::MapPosition position);

// The original's cell window (P:07ad): 287f:1314 copies the 3x3 blocks
// around the party's block into 24x24 cells, row after row. Figures read
// the tile under them from it by index, not from the map, so a place
// beyond its side edge reads a cell of the next or the previous row
// (docs/townspeople.md).
inline constexpr int kWindowCells = 24;
// P:09ed (set by 287f:18ef): the index of the view's top-left cell.
int view_corner_index(formats::MapPosition party);
// The tile at `index` of the window around `party`, nothing for an index
// outside the window (memory the rebuild does not hold).
inline std::optional<int> window_cell(const Terrain& terrain, formats::MapPosition party, int index) {
    return terrain.window_cell(party, index);
}

}  // namespace chr::game
