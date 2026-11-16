// The party's money, and what the town's game tick does to it (docs/town_loop.md).
#pragma once

#include <array>
#include <cstdint>

#include "game/random.h"

namespace chr::game {

struct Money {
    std::int32_t cbills = 20;             // DGROUP:d370, 20 at a new game (1000:4e89)
    std::array<std::int32_t, 3> invested{};  // DGROUP:d374, d378, d37c: three investments
    std::uint16_t floor = 50;             // DGROUP:d33f: 50 at a new game (1000:4f70)
    std::uint8_t countdown = 0;           // DGROUP:d323: the step runs when it passes 0
};

// BTECH.EXE 1000:02ee..0447, part of the main loop's game tick
// (Ghidra FUN_1000_002d's idle half).
// Every 256th game tick (the first one included): in town, while all the
// money is below the floor, 15 C-Bills are added; then each investment
// grows or shrinks at random. Returns true when the C-Bills line is drawn
// again (in town, every time the step runs). `outdoors` is DGROUP:d310.
bool money_tick(Money& money, bool outdoors, Random& random);

}  // namespace chr::game
