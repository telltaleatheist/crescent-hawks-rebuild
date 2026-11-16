// The figures on the map (docs/town_loop.md): which sprite, where, and how
// far it sinks into the ground. Sprites are named by their slot number in
// the program's sprite table (cut from MECHSHAP.CMP at start-up); the
// renderer decides how a slot looks.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/mtp.h"
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
    FigureAnimation();

    // BTECH.EXE 1000:231d (Ghidra FUN_1000_231d)
    // Called by the main loop after every key. A walking key moves each
    // character (present or not) to the next frame of that direction's
    // script, starting the script over if it walked another way before.
    // ('Mechs too, when there are any: not rebuilt yet.)
    void key(int key);

    // A person walking `direction` (0-7, N first, clockwise): starts that
    // direction's character script over unless it is the one it is on, then
    // takes the next frame (1000:24c2 does this for the townspeople).
    void walk(int f, int direction);

    // The frame offset of character `member` (0-7): added to its sprite base.
    int character_frame(int member) const { return frame_[4 + member]; }
    // The frame offset of figure `f`.
    int frame(int f) const { return frame_[std::size_t(f)]; }

    static constexpr int kFigures = 24;

private:
    // BTECH.EXE 1000:1732 (Ghidra FUN_1000_1732)
    // Runs figure `f`'s script to its next frame.
    int step(int f);

    std::array<int, kFigures> pos_{};        // offset into the scripts (37e8:0000 + pos)
    std::array<int, kFigures> direction_{};  // -1: none yet
    std::array<int, kFigures> frame_{};
};

struct Figure {
    AssetId sprite;
    int half_x = 0, half_y = 0;  // the map half-cell it is drawn on (top-left)
    int sink = 0;                // rows cut from the bottom: standing in water
};

// BTECH.EXE 1000:051b (Ghidra FUN_1000_051b), the party's part.
// The present characters, in order, the k-th one drawn at the k-th
// formation place (DGROUP:02ae/02be: by the count drawn so far, not the
// character's own slot). Its sprite is its base (DGROUP:d562) plus its
// animation frame. In shallow water a figure sinks 2 rows, in deeper water
// 4. (The map's objects, the townspeople and the 'Mechs come before and
// after it in the original: not rebuilt yet.)
std::vector<Figure> party_figures(const formats::MtpMap& map, formats::MapPosition position,
                                  const Party& party, const FigureAnimation& animation);

}  // namespace chr::game
