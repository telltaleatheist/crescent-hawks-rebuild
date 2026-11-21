// Draws the overhead map (game::OverheadMap, docs/overhead_map.md). Where it
// sits and how big its small tiles are is layout data the renderer owns.
#pragma once

#include "formats/palette.h"
#include "formats/tables.h"
#include "game/overhead.h"
#include "platform/renderer.h"

namespace chr::platform {

struct OverheadLayout {
    int x = 0, y = 0;   // the top-left small tile, on the 320x200 screen
    int tile_size = 8;  // screen units per small tile; a block's cell is tile_size / 8
};

// The original's: from the screen's corner, 8 x 8 pixels a tile (287f:275c).
inline constexpr OverheadLayout kOriginalOverheadLayout{0, 0, 8};

// The small tiles, then the party's dot. The text goes over them
// (draw_text). False if any tile couldn't be drawn.
bool draw_overhead(Renderer& renderer, const game::OverheadMap& map, const formats::Tables& tables,
                   const formats::Palette16& palette, const OverheadLayout& layout = kOriginalOverheadLayout);

}  // namespace chr::platform
