// The program's sprite table: rectangles cut from MECHSHAP.CMP at start-up
// (docs/town_loop.md, "Sprites"). A slot number names each one.
#pragma once

#include <optional>

namespace chr::formats {

struct SpriteRect {
    int x = 0, y = 0;           // in the 320x200 picture
    int width = 0, height = 0;  // in pixels
};

inline constexpr int kSpriteSlots = 0x178;

// BTECH.EXE 1527:0044 (Ghidra FUN_1527_0044), its calls to
// BTECH.EXE 273d:070a (Ghidra FUN_273d_070a)
// Where slot `slot` comes from in MECHSHAP.CMP, or nothing for a slot the
// start-up never fills.
std::optional<SpriteRect> mechshap_sprite(int slot);

}  // namespace chr::formats
