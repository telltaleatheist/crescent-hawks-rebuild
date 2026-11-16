#include "platform/text_layer.h"

namespace chr::platform {

bool draw_text(Renderer& renderer, const game::TextScreen& text, const formats::Palette16& palette,
               const TextLayout& layout) {
    using Kind = game::TextCell::Kind;
    bool ok = true;
    for (int row = 0; row < game::kTextRows; ++row)
        for (int col = 0; col < game::kTextColumns; ++col) {
            const auto& c = text.cell(col, row);
            if (c.kind == Kind::Empty) continue;
            const int x = layout.left + col * layout.cell, y = layout.top + row * layout.cell;
            if (c.kind == Kind::Frame) {
                ok = renderer.draw(c.asset(), x, y) && ok;
                continue;
            }
            const auto& bg = palette[c.bg & 0x0f];
            renderer.fill(x, y, layout.cell, layout.cell, bg.r, bg.g, bg.b);
            if (c.kind == Kind::Glyph) {
                const auto& fg = palette[c.fg & 0x0f];
                ok = renderer.draw_tinted(c.asset(), x, y, fg.r, fg.g, fg.b) && ok;
            }
        }
    return ok;
}

}  // namespace chr::platform
