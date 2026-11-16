// Microsoft EXEPACK, the packer the shipped BTECH.EXE was put through. Our
// tools/unexepack.py does the same job for analysis; this is the same logic
// in C++ so the program can read data from the player's own BTECH.EXE.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace chr::formats {

// The plain MZ program, byte for byte what tools/unexepack.py writes.
// Throws FormatError if `packed` is not an EXEPACKed MZ file.
std::vector<std::uint8_t> unexepack(std::span<const std::uint8_t> packed);

}  // namespace chr::formats
