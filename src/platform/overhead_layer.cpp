#include "platform/overhead_layer.h"

#include <cstddef>
#include <vector>

namespace chr::platform {

namespace {

// BTECH.EXE 287f:1f51 (Ghidra FUN_287f_1f51), with 1b5d:0327
// A block cell's colour number: its tile number in the cell colour table
// at P:215d. In the cache, 1b5d:0327 has swapped the level's table into
// that place, the one it replaced into the level's (P:2257 for level 0,
// P:2351 for level 1). A tile number past the table's 250 reads on into
// the next one, as the memory holds them.
int cell_colour(const formats::Tables& tables, game::OverheadMap::Colours colours, int tile) {
    constexpr int kLand = 0, kLevel0 = 1, kLevel1 = 2;
    int order[3] = {kLand, kLevel0, kLevel1};  // P:215d, 2257, 2351
    if (colours == game::OverheadMap::Colours::CacheLevel0) order[0] = kLevel0, order[1] = kLand;
    if (colours == game::OverheadMap::Colours::CacheLevel1) order[0] = kLevel1, order[2] = kLand;
    const int table = tile / 250, entry = tile % 250;
    return tables.overhead_colours[std::size_t(order[table])][std::size_t(entry)] & 0x0f;
}

}  // namespace

bool draw_overhead(Renderer& renderer, const game::OverheadMap& map, const formats::Tables& tables,
                   const formats::Palette16& palette, const OverheadLayout& layout) {
    using Tile = game::OverheadMap::Tile;
    const int size = layout.tile_size;
    // 287f:1fbe: the screen black; tiles not seen stay so.
    renderer.fill(layout.x, layout.y, size * game::OverheadMap::kColumns, size * game::OverheadMap::kRows, 0, 0, 0);
    bool drawn = true;
    for (int r = 0; r < game::OverheadMap::kRows; ++r)
        for (int c = 0; c < game::OverheadMap::kColumns; ++c) {
            const auto& tile = map.tiles[std::size_t(r * game::OverheadMap::kColumns + c)];
            const int x = layout.x + c * size, y = layout.y + r * size;
            if (tile.kind == Tile::Kind::Land) {
                drawn = renderer.draw(tile.asset(), x, y) && drawn;
            } else if (tile.kind == Tile::Kind::Block) {
                // 1000:45c2: one pixel a cell.
                const auto& block = map.blocks[std::size_t(tile.number)];
                std::vector<std::uint8_t> rgba(block.size() * 4);
                for (std::size_t i = 0; i < block.size(); ++i) {
                    const auto& rgb = palette[std::size_t(cell_colour(tables, map.colours, block[i]))];
                    rgba[4 * i] = rgb.r, rgba[4 * i + 1] = rgb.g, rgba[4 * i + 2] = rgb.b, rgba[4 * i + 3] = 0xff;
                }
                renderer.draw_rgba(x, y, size, size, rgba);
            }
        }
    // 1000:4553: the dot, in the colour numbered by its random number.
    if (map.dot) {
        const auto& rgb = palette[std::size_t(map.dot->shade & 0x0f)];
        const int unit = size / 8 > 0 ? size / 8 : 1;
        renderer.fill(layout.x + map.dot->x * unit, layout.y + map.dot->y * unit, 2 * unit, 2 * unit, rgb.r, rgb.g, rgb.b);
    }
    return drawn;
}

}  // namespace chr::platform
