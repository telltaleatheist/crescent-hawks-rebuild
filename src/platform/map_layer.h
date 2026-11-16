// Draws a game::MapView. Where the map sits on the screen is layout data the
// renderer owns, so a makeover can move or resize it without touching the
// game logic.
#pragma once

#include <vector>

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

}  // namespace chr::platform
