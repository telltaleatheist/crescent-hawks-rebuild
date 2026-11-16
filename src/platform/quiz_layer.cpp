#include "platform/quiz_layer.h"

#include "game/asset_id.h"

namespace chr::platform {

namespace {
constexpr int kLineX = 0x80;   // 1c67:0aba: the line at x 128, rows 0-192
constexpr int kLineBottom = 0xc0;
constexpr int kLine = 9;       // light blue (10 on CGA, 1c67:0a9d)
constexpr int kMark = 15;      // white
}  // namespace

bool draw_quiz(Renderer& renderer, const game::QuizScene& scene, const formats::Palette16& palette) {
    const bool drawn = renderer.draw(assets::kQuizPicture, 0, 0);
    auto box = [&](int x1, int y1, int x2, int y2, int colour) {  // inclusive corners
        if (x2 < x1) std::swap(x1, x2);
        if (y2 < y1) std::swap(y1, y2);
        const auto& c = palette[std::size_t(colour)];
        renderer.fill(x1, y1, x2 - x1 + 1, y2 - y1 + 1, c.r, c.g, c.b);
    };
    box(kLineX, 0, kLineX, kLineBottom, kLine);
    // 1c67:09ac: a 9x9 square round the part's point, then 273d:01fb from
    // the line to the square, rows +2..+6 of it.
    const int x = scene.x - 3, y = scene.y - 3;
    box(x, y, x + 8, y, kMark);
    box(x, y + 8, x + 8, y + 8, kMark);
    box(x, y, x, y + 8, kMark);
    box(x + 8, y, x + 8, y + 8, kMark);
    box(kLineX, y + 2, x, y + 6, kMark);
    return drawn;
}

}  // namespace chr::platform
