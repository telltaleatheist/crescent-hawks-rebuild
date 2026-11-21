#include "platform/panel_layer.h"

namespace chr::platform {

namespace {

constexpr int kOutline = 14, kFill = 10, kHurt = 4;  // EGA colour numbers (CGA: 3, 1, 2)

}  // namespace

void draw_bars(Renderer& renderer, const std::vector<game::StatBar>& bars, const formats::Palette16& palette) {
    auto box = [&](int x1, int y1, int x2, int y2, int colour) {  // inclusive corners
        const auto& c = palette[std::size_t(colour)];
        renderer.fill(x1, y1, x2 - x1 + 1, y2 - y1 + 1, c.r, c.g, c.b);
    };
    for (const auto& b : bars) {
        // 1000:4aa6: columns 9, 10 and 11 of the member's row, rows 14, 16,
        // 18 and 20; a pixel for each point of the number.
        const int x = (9 + int(b.stat)) * 8, y = (14 + 2 * b.line) * 8;
        box(x + 1, y, x + 5, y, kOutline);
        box(x, y + 1, x, y + 13, kOutline);
        box(x + 6, y + 1, x + 6, y + 13, kOutline);
        box(x + 1, y + 14, x + 5, y + 14, kOutline);
        if (b.value > 0) box(x + 2, y + 14 - b.value, x + 4, y + 13, kFill);
        // 1000:4b42: the hurt part covers the top of the filled part. (A
        // negative one, a number below a tenth of byte 15, would make
        // 273d:01fb draw upside down; what that does is not read yet.)
        if (b.hurt > 0) box(x + 2, y + 14 - b.value, x + 4, y + 14 - b.value + b.hurt, kHurt);
    }
}

}  // namespace chr::platform
