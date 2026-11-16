// Draws a game::MapView. Where the map sits on the screen is layout data the
// renderer owns, so a makeover can move or resize it without touching the
// game logic.
#pragma once

#include <vector>

#include "formats/palette.h"
#include "game/fight.h"
#include "game/figures.h"
#include "game/map_view.h"
#include "platform/renderer.h"

namespace chr::platform {

struct ScreenRect {
    int x = 0, y = 0, width = 0, height = 0;
};

struct MapLayout {
    ScreenRect viewport;   // on the 320x200 screen
    int cell_size = 16;    // screen units per map cell
};

// The original's map window: the right-hand 216x200 of the screen
// (287f:18ef starts drawing 52 bytes, 104 pixels, into the screen buffer).
inline constexpr MapLayout kOriginalMapLayout{{104, 0, 216, 200}, 16};

// False if any tile couldn't be drawn.
bool draw_map(Renderer& renderer, const game::MapView& view,
              const MapLayout& layout = kOriginalMapLayout);

// The figures over the map, in order, each at its half-cell; a sunken figure
// loses that many of the original's pixel rows at the bottom. False if any
// sprite couldn't be drawn.
bool draw_figures(Renderer& renderer, const game::MapView& view, const std::vector<game::Figure>& figures,
                  const MapLayout& layout = kOriginalMapLayout);

// BTECH.EXE 287f:2b87 (Ghidra FUN_287f_2b87) on the map: XORs `colour` into
// the colour number of every pixel of a rectangle of text cells, as the
// original's EGA card does on the screen it shows (the orders screen's box
// on the selected unit). The pixels are worked out again from the map's
// tiles and the figures under the box, by their colour numbers, since two
// numbers can show the same colour. If any of that art has no colour
// numbers (new art) it XORs what the screen shows instead.
bool draw_xor_box(Renderer& renderer, const game::MapView& view, const std::vector<game::Figure>& figures,
                  const game::XorBox& box, const formats::Palette16& palette,
                  const MapLayout& layout = kOriginalMapLayout);

}  // namespace chr::platform
