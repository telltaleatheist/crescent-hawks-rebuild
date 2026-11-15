// .ICN: sheets of 16x16 tiles (docs/formats/icn.md). An .ICN is a .CMP
// container whose 32,000 unpacked bytes are 250 tiles laid end to end.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace chr::formats {

inline constexpr int kTileSize = 16;                                 // pixels, square
inline constexpr int kTileBytes = kTileSize * kTileSize / 2;         // 128, two pixels to a byte
inline constexpr int kTilesPerSheet = 32000 / kTileBytes;            // 250

struct TileSheet {
    std::vector<std::uint8_t> packed;  // 32,000 bytes: tile n at n * 128

    // Tile `index`'s colour numbers (0..15), 16 rows of 16, high nibble first.
    std::array<std::uint8_t, kTileSize * kTileSize> tile(int index) const;
};

TileSheet decode_icn(std::span<const std::uint8_t> file);

}  // namespace chr::formats
