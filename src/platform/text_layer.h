// Draws the game's text screen (game/text.h): each cell's glyph or frame
// piece through the asset provider, glyphs coloured with the cell's colours.
// Where the cells go is the renderer's choice (TextLayout), so a makeover can
// move or resize them.
#pragma once

#include <optional>

#include "formats/palette.h"
#include "game/text.h"
#include "platform/renderer.h"

namespace chr::platform {

struct TextLayout {
    int left = 0, top = 0;  // where cell (0, 0) goes on the 320x200 screen
    // Cell size in screen units; 0 means the font's own size, measured from
    // its glyphs (8x8 for the original font).
    int cell_width = 0, cell_height = 0;
};
inline constexpr TextLayout kOriginalTextLayout{};

// The size the font's characters cover on screen (its widest glyph, from
// the asset provider), or nothing if it has no font.
struct CellSize {
    int width = 0, height = 0;
};
std::optional<CellSize> font_cell_size(Renderer& renderer);

// False (after drawing what it could) if a glyph or frame piece is missing.
bool draw_text(Renderer& renderer, const game::TextScreen& text, const formats::Palette16& palette,
               const TextLayout& layout = kOriginalTextLayout);

}  // namespace chr::platform
