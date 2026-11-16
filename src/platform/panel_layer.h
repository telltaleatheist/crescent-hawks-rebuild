// Draws the panel's bars (game::StatBar). How a bar looks is the renderer's
// choice; this draws the original's: a yellow outline 7 pixels wide and 15
// high with its corners left out, filled green from the bottom, the warning
// part red (1000:4bc1 with 273d:031c lines and 273d:01fb boxes).
#pragma once

#include <vector>

#include "formats/palette.h"
#include "game/town.h"
#include "platform/renderer.h"

namespace chr::platform {

void draw_bars(Renderer& renderer, const std::vector<game::StatBar>& bars, const formats::Palette16& palette);

}  // namespace chr::platform
