// Animated map tiles (docs/town_loop.md): ten tile numbers whose picture
// cycles through three frames from ANIMATE.ICN, one step per game tick.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "formats/tables.h"
#include "game/asset_id.h"

namespace chr::game {

// BTECH.EXE 1000:320b (Ghidra FUN_1000_320b) loads ANIMATE.ICN and keeps its
// tiles 1-30; BTECH.EXE 1000:240b (Ghidra FUN_1000_240b) copies frame
// `phase` of animated tile i (ANIMATE.ICN tile 1 + 3i + phase) over tile
// slot i of DGROUP:04b0's tile numbers (part of BTECH.EXE, now imported:
// tables.json, animated_tiles). So a map cell holding an animated tile shows
// that frame instead of the sheet's own tile. Nothing for other tiles.
inline std::optional<AssetId> animated_tile(std::uint8_t tile, int phase, const formats::Tables& tables) {
    for (std::size_t i = 0; i < tables.animated_tiles.size(); ++i)
        if (tables.animated_tiles[i] == tile) return AssetId{"ANIMATE.ICN", std::uint16_t(1 + 3 * i + phase)};
    return std::nullopt;
}

}  // namespace chr::game

namespace chr::game {

// F:3988 must be 0 for 1000:240b to run. Loading BTTLTECH.ICN
// (1000:4621) sets 0; map 11's DESTRUCT.ICN (1000:2e2e) sets 1, map 14's
// loader (1b5d:...) sets 2. So only the maps drawn with BTTLTECH.ICN animate.
inline bool tiles_animate(int map_number) { return map_number != 11 && map_number != 14; }

}  // namespace chr::game
