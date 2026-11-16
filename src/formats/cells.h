// 8x8 cells the program cuts out of pictures (docs/formats/font.md).
#pragma once

#include <array>
#include <cstdint>

#include "formats/cmp.h"

namespace chr::formats {

inline constexpr int kCellSize = 8;

// BTECH.EXE 2656:0ae5 (Ghidra FUN_2656_0ae5) with 2656:0a3b
// Cell n of a picture cut into 8x8 cells starting at the top-left: cells
// run left to right, 40 to a row. Returns 64 colour numbers.
std::array<std::uint8_t, 64> picture_cell(const CmpPicture& picture, int n);

}  // namespace chr::formats
