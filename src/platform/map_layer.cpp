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

}  // namespace chr::platform
