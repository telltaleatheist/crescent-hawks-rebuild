// A fight's state and the screen until the orders menu (docs/combat.md):
// the 24 figures' tables, and what the fight shows that the town does not.
// The steps themselves are Town members (game/fight.cpp), since a fight
// starts from a place's script and draws on the town's screen.
#pragma once

#include <array>
#include <cstdint>

namespace chr::game {

// The figures' tables in F: (one entry per figure: 0-3 and 12-15 'Mechs,
// 4-11 the party's characters, 16-23 the townspeople, docs/combat.md).
// The walking frames are in FigureAnimation.
struct FightFigures {
    static constexpr int kCount = 24;
    std::array<std::uint16_t, kCount> x{};      // F:4004, ffff: none
    std::array<std::uint16_t, kCount> y{};      // F:4036
    std::array<int, kCount> active{};           // F:406a
    std::array<bool, kCount> destroyed{};       // F:393c
    std::array<std::int8_t, kCount> heading{};  // F:3920
    std::array<int, kCount> sprite_base{};      // F:d55e: the first sprite slot
    std::array<bool, kCount> computer{};        // F:3994: the computer moves it
};

// What a box over a rectangle of text cells marks on the orders screen
// (287f:2b87 on the visible page). The renderer draws each its own way; the
// original look XORs a colour per mark into whatever is there
// (platform/text_layer.cpp).
enum class BoxMark : std::uint8_t {
    Unit = 1,    // the unit being ordered (3x3 cells for a 'Mech, 1 for a person)
    Cursor = 2,  // the waypoint cursor, at the shown path's end
};
// A rectangle of cells and the marks on it, as bits (BoxMark). The original
// XORs, so where two boxes overlap the cells carry both marks and the same
// mark twice cancels: the bits are XORed the same way.
struct XorBox {
    int col = 0, row = 0, width = 0, height = 0;
    std::uint8_t marks = 0;
    bool operator==(const XorBox&) const = default;
};
inline constexpr std::uint8_t mark_bits(BoxMark m) { return std::uint8_t(m); }

// The fight's placement tables were part of BTECH.EXE: DGROUP:3a26/3a2a
// (the k-th 'Mech's place around the party), DGROUP:02de/02e6/02ee/02f6 (the
// cell and water bits under it, for 1000:051b) and DGROUP:1642/164e
// (17dc:0d49's enemy places). They are now imported (tables.json:
// mech_formation_x/y, mech_cell, mech_water, mech_cell_odd_x/y,
// enemy_offset_x/y).

// A 'Mech standing in water sinks (051b, 0e4b): 8 rows, or 16 in deep water.
int mech_sink(int tile, int quarters);

}  // namespace chr::game
