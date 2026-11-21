// The overhead map (docs/overhead_map.md): what the screen shows while the
// command menu's map is up. Tiles by ID, the town blocks as their cells
// (the renderer colours them), and the party's dot. The text (the caption
// and the pointer) is in the town's TextScreen as usual.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "game/asset_id.h"

namespace chr::game {

struct OverheadMap {
    static constexpr int kColumns = 40, kRows = 24;  // small tiles of 8 x 8 in the 320 x 200 layout

    struct Tile {
        enum class Kind : std::uint8_t {
            Hidden,  // not seen yet: nothing drawn
            Land,    // a piece of TINYLAND.CMP (asset())
            Block,   // a town block: blocks[number]
        };
        Kind kind = Kind::Hidden;
        int number = 0;
        AssetId asset() const { return {"TINYLAND.CMP:8x8", std::uint16_t(number)}; }
        bool operator==(const Tile&) const = default;
    };
    std::array<Tile, kColumns * kRows> tiles{};  // row by row

    // A town block drawn small: its 8 x 8 cells' tile numbers, one pixel
    // each in the original look.
    using Block = std::array<std::uint8_t, 64>;
    std::vector<Block> blocks;

    // Which cell colour table the original look uses: the land's, or the
    // cache level's while the party is in the cache (1b5d:0327).
    enum class Colours : std::uint8_t { Land, CacheLevel0, CacheLevel1 };
    Colours colours = Colours::Land;

    // The party: a 2 x 2 dot at (x, y) in the 320 x 200 layout, with this
    // pass's random number (0-15), which the original look takes as its
    // colour. None when the party is off the view.
    struct Dot {
        int x = 0, y = 0;
        int shade = 0;
        bool operator==(const Dot&) const = default;
    };
    std::optional<Dot> dot;

    bool operator==(const OverheadMap&) const = default;
};

}  // namespace chr::game
