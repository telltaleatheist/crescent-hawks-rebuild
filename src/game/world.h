// Positions across the world's areas (docs/formats/mtp.md, "Positions"):
// the low 7 bits of x and y count half-cells within an area, bits 8-11 of x
// and 12-15 of y name the area.
#pragma once

#include <cstdint>

namespace chr::game {

// BTECH.EXE 1000:191b (Ghidra FUN_1000_191b), one half-cell at a time.
// x one half-cell east (+1) or west (-1), into the next area at the edge.
inline std::uint16_t step_x(std::uint16_t x, int d) {
    if (d < 0) {
        x = std::uint16_t(x - 1);
        if (x & 0x80) x &= 0x0f7f;
    } else {
        x = std::uint16_t(x + 1);
        if (x & 0x80) x = std::uint16_t(x + 0x80);
    }
    return x;
}

// y one half-cell south (+1) or north (-1).
inline std::uint16_t step_y(std::uint16_t y, int d) {
    if (d < 0) {
        y = std::uint16_t(y - 1);
        if (y & 0x80) y &= 0xf07f;
    } else {
        y = std::uint16_t(y + 1);
        if (y & 0x80) y = std::uint16_t(y + 0xf80);
    }
    return y;
}

// `n` half-cells at a time (either sign).
inline std::uint16_t move_x(std::uint16_t x, int n) {
    for (; n < 0; ++n) x = step_x(x, -1);
    for (; n > 0; --n) x = step_x(x, 1);
    return x;
}
inline std::uint16_t move_y(std::uint16_t y, int n) {
    for (; n < 0; ++n) y = step_y(y, -1);
    for (; n > 0; --n) y = step_y(y, 1);
    return y;
}

}  // namespace chr::game
