#include "platform/text_layer.h"

#include <algorithm>

namespace chr::platform {

std::optional<CellSize> font_cell_size(Renderer& renderer) {
    CellSize size;
    for (int ch = 0x20; ch < 0x80; ++ch)
        if (const auto s = renderer.logical_size(game::font_glyph(std::uint8_t(ch)))) {
            size.width = std::max(size.width, s->first);
            size.height = std::max(size.height, s->second);
        }
    if (size.width == 0 || size.height == 0) return std::nullopt;
    return size;
}

bool draw_text(Renderer& renderer, const game::TextScreen& text, const formats::Palette16& palette,
               const TextLayout& layout) {
    using Kind = game::TextCell::Kind;
    bool ok = true;
    int cw = layout.cell_width, ch = layout.cell_height;
    if (cw == 0 || ch == 0) {
        static thread_local std::optional<CellSize> measured;  // the font doesn't change
        if (!measured) measured = font_cell_size(renderer);
        if (!measured) return false;
        if (cw == 0) cw = measured->width;
        if (ch == 0) ch = measured->height;
    }
    for (int row = 0; row < game::kTextRows; ++row)
        for (int col = 0; col < game::kTextColumns; ++col) {
            const auto& c = text.cell(col, row);
            if (c.kind == Kind::Empty) continue;
            const int x = layout.left + col * cw, y = layout.top + row * ch;
            if (c.kind == Kind::Frame) {
                ok = renderer.draw(c.asset(), x, y) && ok;
                continue;
            }
            const auto& bg = palette[c.bg & 0x0f];
            renderer.fill(x, y, cw, ch, bg.r, bg.g, bg.b);
            if (c.kind == Kind::Glyph) {
                const auto& fg = palette[c.fg & 0x0f];
                ok = renderer.draw_tinted(c.asset(), x, y, fg.r, fg.g, fg.b) && ok;
            }
        }
    return ok;
}

}  // namespace chr::platform
