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
    // BTECH.EXE 1000:522c sets the state to 25 13 90 at start-up.
    // (1000:a9de seeds it again from DGROUP:d456 somewhere not yet traced.)
    Random() = default;
    explicit Random(std::array<std::uint8_t, 3> state) : s_(state) {}

    // BTECH.EXE 287f:0bc0 (Ghidra FUN_287f_0bc0)
    // The next number, 0..255.
    std::uint8_t next();

    const std::array<std::uint8_t, 3>& state() const { return s_; }

private:
    std::array<std::uint8_t, 3> s_{0x25, 0x13, 0x90};
};

}  // namespace chr::game
