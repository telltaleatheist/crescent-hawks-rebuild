// The script flags: the bytes from DGROUP:d30c (docs/scripts.md). The .BLD
// scripts read and write them by number, and the rest of the program keeps
// some of its own state in the same bytes, so they are one array here too.
// The scripts use 00-3f; C-Bills follow at d370 (flag 64).
#pragma once

#include <array>
#include <cstdint>

namespace chr::game {

inline constexpr int kFlagCount = 0x64;
using Flags = std::array<std::uint8_t, kFlagCount>;

namespace flag {
inline constexpr int kOutdoors = 0x04;        // d310
inline constexpr int kCourse = 0x08;          // d314: the course chosen (special 05)
inline constexpr int kEnrolled = 0x09;        // d315: special 05's answer
inline constexpr int kInPlace = 0x0a;         // d316: set on entering a place
inline constexpr int kPlace = 0x0d;           // d319: which place, for talking
inline constexpr int kMembers = 0x0e;         // d31a: members besides Jason (special 14)
inline constexpr int kMoneyCountdown = 0x17;  // d323: the money step runs when it passes 0
inline constexpr int kLetOut = 0x2d;          // d339
inline constexpr int kLetOutDone = 0x2e;      // d33a
inline constexpr int kMoneyFloor = 0x33;      // d33f-d340: a word; the training center raises it
inline constexpr int kPictureChance = 0x4f;   // d35b: how often a place's picture shows
}  // namespace flag

// A new game's flags: the money floor is 50 (1000:4f70); the rest are 0.
inline Flags new_game_flags() {
    Flags f{};
    f[flag::kMoneyFloor] = 50;
    return f;
}

inline int flag_word(const Flags& f, int at) { return f[std::size_t(at)] | f[std::size_t(at) + 1] << 8; }

}  // namespace chr::game
