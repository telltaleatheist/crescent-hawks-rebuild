#include "formats/mtp.h"

#include "formats/cmp.h"  // FormatError

namespace chr::formats {

namespace {

constexpr std::size_t kTablesSize = 5 + 0x80 + 0x100 + 4 * 0x20 + 0x10 + 8;  // 0x21d

std::string fixed_string(std::span<const std::uint8_t> bytes) {
    std::string s;
    for (auto b : bytes) {
        if (b == 0) break;
        s.push_back(char(b));
    }
    return s;
}

std::uint16_t word(std::span<const std::uint8_t> bytes, std::size_t at) {
    return std::uint16_t(bytes[at] | (bytes[at + 1] << 8));
}

}  // namespace

// BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8)
// Loads MAP<n>.MTP field by field: five header bytes, the people's and the
// places' names, two pairs of 16-entry position tables, two small tables,
// then width*height cell bytes into the block store from block 16 on. It
// then writes 0x90, 0x91, ... (block 16 and up, as "fixed block" entries)
// into the area's block grid, starting at (block_x, block_y).
MtpMap decode_mtp(std::span<const std::uint8_t> file) {
    if (file.size() < kTablesSize) throw FormatError("MTP: shorter than its tables");
    MtpMap map;
    map.unknown_first_byte = file[0];
    map.block_x = file[1];
    map.block_y = file[2];
    map.width = file[3];
    map.height = file[4];
    if (map.width == 0 || map.height == 0 || map.width % kBlockCells || map.height % kBlockCells ||
        map.width > 64 || map.height > 64)
        throw FormatError("MTP: not a block map (size " + std::to_string(map.width) + "x" +
                          std::to_string(map.height) + ")");
    const std::size_t cells = std::size_t(map.width) * map.height;
    if (file.size() != kTablesSize + cells)
        throw FormatError("MTP: size doesn't match " + std::to_string(map.width) + "x" +
                          std::to_string(map.height));

    std::size_t at = 5;
    for (auto& name : map.people) name = fixed_string(file.subspan(at, 16)), at += 16;
    for (auto& name : map.places) name = fixed_string(file.subspan(at, 16)), at += 16;
    for (auto& p : map.entrances) p.x = word(file, at), at += 2;
    for (auto& p : map.entrances) p.y = word(file, at), at += 2;
    for (auto& p : map.second_positions) p.x = word(file, at), at += 2;
    for (auto& p : map.second_positions) p.y = word(file, at), at += 2;
    for (auto& b : map.place_scripts) b = file[at++];
    for (auto& b : map.start_shuffle) b = file[at++];
    map.blocks.assign(file.begin() + std::ptrdiff_t(at), file.end());
    return map;
}

// BTECH.EXE 287f:13d9 (Ghidra FUN_287f_13d9)
// For a fixed block the view builder copies its 64 bytes as 8 rows of 8.
std::uint8_t MtpMap::cell(int x, int y) const {
    const int blocks_across = width / kBlockCells;
    const int block = (y / kBlockCells) * blocks_across + x / kBlockCells;
    return blocks[std::size_t(block) * 64 + (y % kBlockCells) * kBlockCells + x % kBlockCells];
}

}  // namespace chr::formats
