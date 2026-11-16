#include "platform/map_layer.h"

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

}  // namespace chr::platform
