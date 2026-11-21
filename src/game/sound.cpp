#include "game/sound.h"

#include <cstddef>
#include <cstdint>

namespace chr::game {

namespace {

constexpr int kKind = 0x3e8;  // codes above it are kinds 1-4; up to it, notes

// How many words an entry with this code takes. The code is compared
// signed, so a word of 0x8000 or more is a note.
std::size_t entry_size(int word) {
    const int code = std::int16_t(word);
    if (code <= kKind) return 3;
    switch (code - kKind) {
    case 1: return 6;
    case 2:
    case 3:
    case 4: return 7;
    default: return 1;
    }
}

}  // namespace

void play_entries(SoundState& state, std::span<const int> lists, int n) {
    const auto word = [&](std::size_t i) { return i < lists.size() ? lists[i] & 0xffff : 0; };
    const auto ends = [&](std::size_t i) { return word(i + 1) == 0 && word(i + 2) == 0; };
    // Step over n - 1 effects: each entry followed by two 0 words ends one.
    int skip = n - 1;
    std::size_t i = 0;
    while (word(i) != 0) {
        if (skip == 0) break;
        if (ends(i)) --skip;
        i += entry_size(word(i));
    }
    if (skip != 0) return;
    while (!ends(i)) {
        const int code = word(i);
        if (code == kKind + 2)
            for (std::size_t k = 0; k < state.sweep.size(); ++k) state.sweep[k] = std::uint16_t(word(i + 2 + k));
        i += entry_size(code);
    }
}

}  // namespace chr::game
