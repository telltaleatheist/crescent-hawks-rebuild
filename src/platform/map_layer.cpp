#include "platform/map_layer.h"

#include <algorithm>

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

}  // namespace chr::platform
