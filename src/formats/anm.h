// The places' pictures: O<n>.ANM (docs/formats/anm.md). An 88x88 picture,
// 16 colours, as a first frame and changes to it: each frame is XORed onto
// the one before, from a blank start.
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace chr::formats {

inline constexpr int kAnmSize = 88;                          // pixels, square
inline constexpr int kAnmFrameBytes = kAnmSize * kAnmSize / 2;  // 3872, two pixels a byte

struct AnmPicture {
    // The frame list, one entry per frame shown: entry k names frame k's
    // delay. The first frame always shows; the list ends at a 0 after it.
    std::vector<int> sequence;
    int speed = 0;  // +32: the delay of every frame is scaled by it
    // The frames as shown, each the colour number of every pixel (88x88,
    // row by row, 0-15), after XORing the frame's changes onto the one
    // before.
    std::vector<std::vector<std::uint8_t>> frames;
    // BTECH.EXE 1000:1afd: the ticks frame k stays up,
    // delay(sequence[k]) * speed * 3 / 4.
    std::vector<int> delays;
};

// BTECH.EXE 1000:48b7, 1000:1afd, 287f:23ec (Ghidra FUN_1000_48b7,
// FUN_1000_1afd, FUN_287f_23ec). Throws FormatError for a short or broken
// file.
AnmPicture decode_anm(const std::vector<std::uint8_t>& file);
AnmPicture read_anm(const std::filesystem::path& path);

}  // namespace chr::formats
