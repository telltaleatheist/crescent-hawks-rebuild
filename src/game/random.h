// The original's random numbers: a 3-byte shift register (DGROUP:4fc0..4fc2).
// Every game tick of the town's main loop draws one, so the sequence a
// player meets depends on how long they wait; the rebuild draws them at the
// same moments to keep that.
#pragma once

#include <array>
#include <cstdint>

namespace chr::game {

class Random {
public:
    // The state in the program image: 04 03 02. (1000:a9de seeds it again
    // from DGROUP:d456 in a wait loop not traced yet.)
    Random() = default;
    explicit Random(std::array<std::uint8_t, 3> state) : s_(state) {}

    // BTECH.EXE 287f:0bc0 (Ghidra FUN_287f_0bc0)
    // The next number, 0..255.
    std::uint8_t next();

    const std::array<std::uint8_t, 3>& state() const { return s_; }

    // The state when a new game reaches the town's map load: the image's
    // 04 03 02 after 264 draws: 256 fill a table at start-up (1000:50c8,
    // DGROUP(5400):09fb), 8 pick the 'Mechs' looks in the new game (1000:4f41:
    // sprite base 0x92 or 0 for figures 0-3 and 12-15). Checked against the
    // original's memory with the reference rig.
    static Random at_town_start();

private:
    std::array<std::uint8_t, 3> s_{0x04, 0x03, 0x02};
};

}  // namespace chr::game
