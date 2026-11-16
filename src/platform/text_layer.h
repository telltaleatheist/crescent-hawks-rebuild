// Draws the game's text screen (game/text.h): each cell's glyph or frame
// piece through the asset provider, glyphs coloured with the cell's colours.
// Where the cells go is the renderer's choice (TextLayout), so a makeover can
// move or resize them.
#pragma once

#include "formats/palette.h"
#include "game/text.h"
#include "platform/renderer.h"

namespace chr::platform {

struct TextLayout {
    int left = 0, top = 0;  // where cell (0, 0) goes on the 320x200 screen
    int cell = 8;           // cell size in screen units
};
inline constexpr TextLayout kOriginalTextLayout{};

// False (after drawing what it could) if a glyph or frame piece is missing.
bool draw_text(Renderer& renderer, const game::TextScreen& text, const formats::Palette16& palette,
               const TextLayout& layout = kOriginalTextLayout);

}  // namespace chr::platform
