#include "platform/map_layer.h"

#include <algorithm>
#include <cstdlib>

#include "game/combat.h"

namespace chr::platform {

bool draw_map(Renderer& renderer, const game::MapView& view, const MapLayout& layout) {
    const int half = layout.cell_size / 2;
    renderer.clip(layout.viewport.x, layout.viewport.y, layout.viewport.width,
                  layout.viewport.height);
    bool all = true;
    for (const auto& cell : view.cells) {
        const int x = layout.viewport.x + cell.cell_x * layout.cell_size - view.origin_half_x * half;
        const int y = layout.viewport.y + cell.cell_y * layout.cell_size - view.origin_half_y * half;
        all = renderer.draw(cell.tile, x, y) && all;
    }
    renderer.clear_clip();
    return all;
}

bool draw_figures(Renderer& renderer, const game::MapView& view, const std::vector<game::Figure>& figures,
                  const MapLayout& layout) {
    const int half = layout.cell_size / 2;
    renderer.clip(layout.viewport.x, layout.viewport.y, layout.viewport.width,
                  layout.viewport.height);
    bool all = true;
    for (const auto& f : figures) {
        const int x = layout.viewport.x + (f.half_x - view.origin_half_x) * half;
        const int y = layout.viewport.y + (f.half_y - view.origin_half_y) * half;
        const auto size = renderer.logical_size(f.sprite);
        if (!size) {
            all = false;
            continue;
        }
        // The sink is in the original's pixels: half a cell is 8 of them.
        all = renderer.draw_top(f.sprite, x, y, size->second - f.sink * half / 8) && all;
    }
    renderer.clear_clip();
    return all;
}

namespace {

// Copies an image's colour numbers into `out` (the box's pixels, from
// screen point (bx, by)), the top `visible` rows of it, skipping see-through
// ones. False if the image has none or is not at the original's size.
bool blit_numbers(const Image& image, int x, int y, int visible, std::vector<std::uint8_t>& out, int bx, int by,
                  int bw, int bh) {
    if (image.numbers.empty() || image.width != image.logical_width || image.height != image.logical_height)
        return false;
    for (int iy = 0; iy < std::min(visible, image.height); ++iy)
        for (int ix = 0; ix < image.width; ++ix) {
            const int sx = x + ix - bx, sy = y + iy - by;
            if (sx < 0 || sy < 0 || sx >= bw || sy >= bh) continue;
            const auto n = image.numbers[std::size_t(iy * image.width + ix)];
            if (n != 0xff) out[std::size_t(sy * bw + sx)] = n;
        }
    return true;
}

bool overlaps(int x, int y, int w, int h, int bx, int by, int bw, int bh) {
    return x < bx + bw && x + w > bx && y < by + bh && y + h > by;
}

}  // namespace

bool draw_xor_box(Renderer& renderer, const game::MapView& view, const std::vector<game::Figure>& figures,
                  const game::XorBox& box, const formats::Palette16& palette, const MapLayout& layout) {
    const int bx = 8 * box.col, by = 8 * box.row, bw = 8 * box.width, bh = 8 * box.height;
    const int half = layout.cell_size / 2;
    std::vector<std::uint8_t> numbers(std::size_t(bw * bh), 0xff);
    bool known = true;
    for (const auto& cell : view.cells) {
        const int x = layout.viewport.x + cell.cell_x * layout.cell_size - view.origin_half_x * half;
        const int y = layout.viewport.y + cell.cell_y * layout.cell_size - view.origin_half_y * half;
        if (!overlaps(x, y, layout.cell_size, layout.cell_size, bx, by, bw, bh)) continue;
        const Image* image = renderer.source(cell.tile);
        known = known && image && blit_numbers(*image, x, y, image->height, numbers, bx, by, bw, bh);
    }
    for (const auto& f : figures) {
        const int x = layout.viewport.x + (f.half_x - view.origin_half_x) * half;
        const int y = layout.viewport.y + (f.half_y - view.origin_half_y) * half;
        const Image* image = renderer.source(f.sprite);
        if (!image) {
            known = false;
            continue;
        }
        if (!overlaps(x, y, image->width, image->height, bx, by, bw, bh)) continue;
        known = known && blit_numbers(*image, x, y, image->height - f.sink * half / 8, numbers, bx, by, bw, bh);
    }
    known = known && std::find(numbers.begin(), numbers.end(), std::uint8_t(0xff)) == numbers.end();
    if (!known) return renderer.xor_colours(bx, by, bw, bh, box.colour, palette);
    std::vector<std::uint8_t> rgba(numbers.size() * 4);
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        const auto& c = palette[std::size_t((numbers[i] ^ box.colour) & 0x0f)];
        rgba[4 * i] = c.r, rgba[4 * i + 1] = c.g, rgba[4 * i + 2] = c.b, rgba[4 * i + 3] = 0xff;
    }
    renderer.draw_rgba(bx, by, bw, bh, rgba);
    return true;
}

namespace {

// The original look's colour number for a shot's line (22e8:000c): a hand
// laser yellow, light green for an enemy person's (CGA: 3); a 'Mech's beam
// light red for the party's, magenta for an enemy's (CGA: 2).
int line_colour(const game::ShownLine& line) {
    using Kind = game::ShownLine::Kind;
    using Shooter = game::ShownLine::Shooter;
    if (line.kind == Kind::HandLaser) return line.shooter == Shooter::EnemyPerson ? 10 : 14;
    return line.shooter == Shooter::Party ? 12 : 5;
}

// BTECH.EXE 22e8:000c (Ghidra FUN_22e8_000c), the line's part
// The points of a line as the original plots them (273d:031c): from its
// start, a straight step and, when the error runs out, a diagonal one
// (DG:41dc by octant), until it reaches its end; only points inside the
// map's side of the screen are plotted.
template <class Plot>
void walk_line(const game::ShownLine& line, const formats::Tables& tables, Plot plot) {
    int x = line.from_x, y = line.from_y;
    const int tx = line.to_x, ty = line.to_y;
    const int dy = y - ty, dx = tx - x;
    int major = std::abs(dy), minor = std::abs(dx);
    if (minor > major) std::swap(major, minor);
    const int o = game::octant(dy, dx);
    const int step_x = tables.line_steps[std::size_t(o)], step_y = tables.line_steps[std::size_t(8 + o)];
    const int diag_x = tables.line_steps[std::size_t(16 + o)], diag_y = tables.line_steps[std::size_t(24 + o)];
    int error = major >> 1;
    for (int guard = 0; (x != tx || y != ty) && guard <= 0x4000; ++guard) {
        if (x >= 0x68 && x < 0x140 && y >= 0 && y < 200) plot(x, y);
        if (x != tx) x += step_x;
        if (y != ty) y += step_y;
        error -= minor;
        if (error < 0) {
            if (x != tx) x += diag_x;
            if (y != ty) y += diag_y;
            error += major;
        }
    }
}

}  // namespace

bool draw_shot_marks(Renderer& renderer, const std::vector<game::ShownSprite>& sprites,
                     const std::vector<game::ShownLine>& lines, const formats::Tables& tables,
                     const formats::Palette16& palette, const MapLayout& layout) {
    const auto& o = kOriginalMapLayout.viewport;
    const auto& v = layout.viewport;
    const auto at_x = [&](int x) { return v.x + (x - o.x) * v.width / o.width; };
    const auto at_y = [&](int y) { return v.y + (y - o.y) * v.height / o.height; };
    renderer.clip(v.x, v.y, v.width, v.height);
    bool all = true;
    for (const auto& s : sprites) all = renderer.draw(s.sprite, at_x(s.x), at_y(s.y)) && all;
    const int w = std::max(1, v.width / o.width), h = std::max(1, v.height / o.height);
    for (const auto& line : lines) {
        const auto& c = palette[std::size_t(line_colour(line) & 15)];
        walk_line(line, tables, [&](int x, int y) { renderer.fill(at_x(x), at_y(y), w, h, c.r, c.g, c.b); });
    }
    renderer.clear_clip();
    return all;
}

}  // namespace chr::platform
