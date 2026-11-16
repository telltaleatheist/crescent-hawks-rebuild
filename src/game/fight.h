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

// A character record's bytes the fight reads (F:c614 + 0x11 i): byte 0, the
// name number (ff: no character), and byte 0x0c, where the character is: a
// 'Mech record 0-7, or 8 and above on foot.
struct CharacterPlace {
    std::uint8_t name = 0xff;
    std::uint8_t mech = 0;
};

// A box drawn by XORing a colour into every pixel of a rectangle of text
// cells on the screen (287f:2b87 on the visible page): the orders screen's
// mark on the selected unit. The renderer flips the colours of whatever is
// there.
struct XorBox {
    int col = 0, row = 0, width = 0, height = 0;
    int colour = 0;
    bool operator==(const XorBox&) const = default;
};

// The fight's placement tables were part of BTECH.EXE: DGROUP:3a26/3a2a
// (the k-th 'Mech's place around the party), DGROUP:02de/02e6/02ee/02f6 (the
// cell and water bits under it, for 1000:051b) and DGROUP:1642/164e
// (17dc:0d49's enemy places). They are now imported (tables.json:
// mech_formation_x/y, mech_cell, mech_water, mech_cell_odd_x/y,
// enemy_offset_x/y).

// A 'Mech standing in water sinks (051b, 0e4b): 8 rows, or 16 in deep water.
int mech_sink(int tile, int quarters);

}  // namespace chr::game
