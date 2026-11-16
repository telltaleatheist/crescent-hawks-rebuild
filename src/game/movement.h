// Walking on a map, as the original's step check does it (docs/movement.md).
// Positions are in half-cells; one key press moves one half-cell on each
// axis it names.
#pragma once

#include <array>
#include <optional>

#include "formats/mtp.h"
#include "formats/tables.h"

namespace chr::game {

// Keys in the program's own codes: a character, or minus the scan code for
// the keys that have none (what 273d:0259 hands back).
namespace key {
inline constexpr int kHome = -0x47, kUp = -0x48, kPageUp = -0x49, kLeft = -0x4b, kRight = -0x4d,
                     kEnd = -0x4f, kDown = -0x50, kPageDown = -0x51, kEnter = 0x0d, kSpace = 0x20;
}

struct Step {
    int dx = 0, dy = 0;
};

// BTECH.EXE 1000:218f (Ghidra FUN_1000_218f), first part.
// The direction a key asks for: the arrows, and Home, PgUp, End and PgDn for
// the diagonals (the numeric keypad).
std::optional<Step> step_for_key(int key);

// BTECH.EXE 2656:0d1d (Ghidra FUN_2656_0d1d)
// The other keys a menu or a cursor takes as arrows (see movement.cpp).
int arrow_key(int k);

// Who is walking: up to eight characters, each at a fixed offset from the
// party's position (DGROUP:0464/047c, in half-cells: imported into
// tables.json as formation_x/y). A new game has one.
struct Party {
    std::array<bool, 8> present{true, false, false, false, false, false, false, false};
};

// The lowest tile number that blocks a step (DGROUP:0150, set by 1000:2da8).
int blocking_tile(int map_number);

enum class StepOutcome {
    Moved,
    Blocked,    // a character would stand on a blocking tile
    TooDeep,    // the block it would enter is deep water ("The water is too deep that way.")
    LeftMap,    // it would leave the map: the original walks on into the world (docs/movement.md)
};

// BTECH.EXE 1000:1c12 (Ghidra FUN_1000_1c12)
// One step. `ask_enter(place)` is called when a character would land exactly
// on an entrance (one of the first 12); it shows the question and returns
// the answer. The step is still checked afterwards, as in the original.
struct StepResult {
    StepOutcome outcome = StepOutcome::Moved;
    formats::MapPosition position;     // after the step
    std::optional<int> offered_place;  // the entrance that was asked about
    bool entered = false;              // and the answer was yes
};

class EntranceQuestion {
public:
    virtual ~EntranceQuestion() = default;
    virtual bool ask(int place) = 0;
};

StepResult take_step(const formats::MtpMap& map, int map_number, formats::MapPosition from,
                     const Party& party, Step step, EntranceQuestion& question,
                     const formats::Tables& tables);

}  // namespace chr::game
