// 8x8 pieces: the font's characters and the cells the program cuts out of
// pictures (docs/formats/font.md).
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "formats/cmp.h"

namespace chr::formats {

inline constexpr int kCellSize = 8;

// BTECH.EXE 287f:2251 (Ghidra FUN_287f_2251) draws one character on EGA.
// The font: 8 bytes per character, a byte per row, the top bit on the left.
// Returns 64 values, 1 where the character's pixel is set.
std::array<std::uint8_t, 64> font_character(std::span<const std::uint8_t> font, int ch);

// BTECH.EXE 2656:0ae5 (Ghidra FUN_2656_0ae5) with 2656:0a3b
// Cell n of a picture cut into 8x8 cells starting at the top-left: cells
// run left to right, 40 to a row. Returns 64 colour numbers.
std::array<std::uint8_t, 64> picture_cell(const CmpPicture& picture, int n);

}  // namespace chr::formats
