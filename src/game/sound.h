// What the sound player leaves behind (docs/sound.md). The logic plays no
// audio: it names the effect by number and keeps the words the original's
// player stores in the far data, because the game reads one back.
#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace chr::game {

struct SoundState {
    bool on = true;  // DG:015c

    // The last kind-2 entry's five words: F:0000, F:39f4, F:4000, F:4034,
    // F:0062 (0 in the program image).
    std::array<std::uint16_t, 5> sweep{};

    // Figure -1's position (F:4002, F:4034): x is never written.
    std::uint16_t minus_one_x() const { return 0; }
    std::uint16_t minus_one_y() const { return sweep[3]; }
};

// BTECH.EXE 27c5:0002 (Ghidra FUN_27c5_0002)
// Effect n's entries from the lists (DG:5008), with what they store: here
// only the kind-2 words. The speaker routines are not rebuilt.
void play_entries(SoundState& state, std::span<const int> lists, int n);

}  // namespace chr::game
