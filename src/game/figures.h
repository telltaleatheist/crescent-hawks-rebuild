// The figures on the map (docs/town_loop.md): which sprite, where, and how
// far it sinks into the ground. Sprites are named by their slot number in
// the program's sprite table (cut from MECHSHAP.CMP at start-up); the
// renderer decides how a slot looks.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/mtp.h"
#include "formats/tables.h"
#include "game/terrain.h"
#include "game/asset_id.h"
#include "game/movement.h"

namespace chr::game {

// Sprite slot n of the program's table (1527:0044 fills it with 273d:070a).
inline AssetId sprite(int slot) { return {"MECHSHAP.CMP:sprite", std::uint16_t(slot)}; }

// Each figure's walking animation: a little script per direction that
// gives the next frame on every key (DGROUP:01f6 holds where each figure is
// in its script, 0396c the direction it last walked, 409a the frame).
// Figures 0-3 are the 'Mechs, 4-11 the party's eight characters, 12-15
// more 'Mechs and 16-23 the map's eight townspeople.
class FigureAnimation {
public:
    // BTECH.EXE 1000:5020..50aa (part of the new-game setup): every figure
    // starts on the walking-south script without having stepped through it,
    // frame 0 for the 'Mechs and 0x10 for the characters (and, set by the
    // map load 1000:2da8, for the townspeople).
    // The walking scripts and keys come from the imported tables, which must
    // outlive it.
    explicit FigureAnimation(const formats::Tables& tables);

    // BTECH.EXE 1000:231d (Ghidra FUN_1000_231d)
    // Called by the main loop after every key. A walking key moves each
    // character (present or not) to the next frame of that direction's
    // script, starting the script over if it walked another way before
    // (the 'Mechs' part: docs/town_loop.md). The 'Mech figures 0-3 whose
    // records exist (`mechs`) walk too, with the 'Mech scripts (DG:025a).
    void key(int key, const std::array<bool, 4>& mechs = {});

    // A person walking `direction` (0-7, N first, clockwise): starts that
    // direction's character script over unless it is the one it is on, then
    // takes the next frame (1000:24c2 does this for the townspeople).
    void walk(int f, int direction);

    // Puts figure `f` at the start of the script at `address` (the constant
    // segment's offset, 0x270-0x2ef: DGROUP:01f6 + 4f), as special 1 and
    // 17dc:0d49 do for the 'Mechs.
    void start_script(int f, int address);
    // Runs figure `f`'s script to its next frame and makes it the frame
    // (F:409a), as 1000:1732's callers do.
    void advance(int f) { frame_[std::size_t(f)] = step(f); }
    // The script byte at `address` (37e8:0270..02ef, or the data segment's
    // shot scripts); 0xff past them.
    int script_byte(int address) const;
    // Runs the script at `pos` to its next frame, as 1000:1732 does for a
    // figure (a direction byte goes to *direction); -1 at an ff.
    int run(int& pos, int* direction = nullptr) const;
    // Where figure f is in its script (DGROUP:01f6 + 4f), and whether it is
    // at an ff, a pose's end.
    int script_position(int f) const { return pos_[std::size_t(f)]; }
    bool at_end(int f) const { return script_byte(pos_[std::size_t(f)]) == 0xff; }
    // Sets a frame and the direction last walked (F:396c) directly.
    void set_frame(int f, int frame) { frame_[std::size_t(f)] = frame; }
    void set_direction(int f, int direction) { direction_[std::size_t(f)] = direction; }
    // The direction figure f last walked (F:396c), -1 for none.
    int direction(int f) const { return direction_[std::size_t(f)]; }
    // The 'Mechs' script for a direction (0x270 + 8d).
    static constexpr int mech_script(int direction) { return 0x270 + 8 * direction; }

    // The frame offset of character `member` (0-7): added to its sprite base.
    int character_frame(int member) const { return frame_[4 + member]; }
    // The frame offset of figure `f`.
    int frame(int f) const { return frame_[std::size_t(f)]; }

    static constexpr int kFigures = 24;

private:
    // BTECH.EXE 1000:1732 (Ghidra FUN_1000_1732)
    // Runs figure `f`'s script to its next frame.
    int step(int f);

    const formats::Tables* tables_;
    std::array<int, kFigures> pos_{};        // the script's offset (37e8:0270.., or DG for the shot's)
    std::array<int, kFigures> direction_{};  // -1: none yet
    std::array<int, kFigures> frame_{};
};

struct Figure {
    AssetId sprite;
    int half_x = 0, half_y = 0;  // the map half-cell it is drawn on (top-left)
    int sink = 0;                // rows cut from the bottom: standing in water
};

// BTECH.EXE 1000:051b, 1000:0d30, 1000:0e4b and 1000:2a93 all place a
// figure the same way: its column (x - party x + 0x1a) and row
// (y - party y + 0x0c), each & 0x7f, times 8 pixels (docs/combat.md 3.13).
// As a half-cell in the party's own area, for the renderer.
inline int folded_half_x(int col, std::uint16_t party_x) { return (col & 0x7f) + (party_x & 0x7f) - 0x1a; }
inline int folded_half_y(int row, std::uint16_t party_y) { return (row & 0x7f) + (party_y & 0x7f) - 0x0c; }

// BTECH.EXE 1000:051b (Ghidra FUN_1000_051b), the party's part.
// The present characters, in order, the k-th one drawn at the k-th
// formation place (DGROUP:02ae/02be: by the count drawn so far, not the
// character's own slot). Its sprite is its base (F:d562) plus its
// animation frame. In shallow water a figure sinks 2 rows, in deeper water
// 4. (The map's objects, the townspeople and the 'Mechs come before and
// after it: docs/town_loop.md.)
std::vector<Figure> party_figures(const Terrain& terrain, formats::MapPosition position,
                                  const Party& party, const FigureAnimation& animation,
                                  const formats::Tables& tables, const std::array<int, 8>& sprite_base);

}  // namespace chr::game
