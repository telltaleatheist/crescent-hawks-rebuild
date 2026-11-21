// Draws the game's text screen (game/text.h): each cell's glyph or frame
// piece through the asset provider, glyphs coloured with the cell's colours.
// Where the cells go is the renderer's choice (TextLayout), so a makeover can
// move or resize them.
#pragma once

#include <optional>

#include "formats/palette.h"
#include "formats/tables.h"
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

// The original look's colour number for a text colour: a data colour as
// it is, a role from the original look's table (text_layer.cpp).
int colour_number(game::TextColour colour);

// The original look's XOR colour for a box's marks (game::BoxMark bits):
// each mark's colour, XORed together as the original's boxes are.
int box_marks_colour(std::uint8_t marks);

// False (after drawing what it could) if a glyph or frame piece is missing.
// A menu's highlight bar, in the original look, XORs the menu's highlight
// colour (its record in `tables`) into the cells under it.
bool draw_text(Renderer& renderer, const game::TextScreen& text, const formats::Tables& tables,
               const formats::Palette16& palette, const TextLayout& layout = kOriginalTextLayout);

}  // namespace chr::platform
