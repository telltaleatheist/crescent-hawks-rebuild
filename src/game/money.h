// The party's money, and what the town's game tick does to it (docs/town_loop.md).
#pragma once

#include <array>
#include <cstdint>

#include "game/flags.h"
#include "game/random.h"

namespace chr::game {

struct Money {
    std::int32_t cbills = 20;             // F:d370, 20 at a new game (1000:4e89)
    std::array<std::int32_t, 3> invested{};  // F:d374, d378, d37c: three investments
    // The floor (F:d33f) and the countdown (d323) are script flags
    // (game/flags.h): the training center raises the floor.
};

// BTECH.EXE 1000:02ee..0447, part of the main loop's game tick
// (Ghidra FUN_1000_002d's idle half).
// Every 256th game tick (the first one included): in town, while all the
// money is below the floor, 15 C-Bills are added; then each investment
// grows or shrinks at random. Returns true when the C-Bills line is drawn
// again (in town, every time the step runs). `outdoors` is F:d310.
bool money_tick(Money& money, Flags& flags, bool outdoors, Random& random);

}  // namespace chr::game
