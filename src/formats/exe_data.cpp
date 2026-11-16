#include "formats/exe_data.h"

#include <algorithm>
#include <array>

#include "formats/cmp.h"  // FormatError
#include "formats/exepack.h"
#include "formats/game_files.h"
#include "formats/sha256.h"

namespace chr::formats {

namespace {

// DOS 1.03. DGROUP (the program's data segment) is 46db; the asm display
// library's data is 2c6c. Text pieces keep the program's control bytes
// (docs/text.md); several of them are not NUL-free, so each has its length.
constexpr std::array kDos103 = {
    // 2c6c:a661, 1,024 bytes: the 8x8 font, one bit per pixel, 8 bytes per
    // character, characters 0..127 (273d:00d5 masks with 0x7f).
    ExeSpan{"font", 0x2c6c, 0xa661, 0x400},
    // 1000:1c12, the step check.
    ExeSpan{"text.too_deep", 0x46db, 0x0410, 32},       // "The water is too deep that way."
    ExeSpan{"text.enter_before", 0x46db, 0x0430, 20},   // "Will you enter the\r"
    ExeSpan{"text.enter_after", 0x46db, 0x0444, 2},     // "?"
    // 1000:1a13, the yes/no question: the answer line, Yes picked / No picked.
    ExeSpan{"text.yes_picked", 0x46db, 0x03ee, 16},
    ExeSpan{"text.no_picked", 0x46db, 0x03fe, 17},
    // 1000:4cac, the party box heading.
    ExeSpan{"text.party_heading", 0x46db, 0x0aca, 4},  // "BDC"
    // The eight characters' names (pointers at DGROUP:01ca).
    ExeSpan{"name.0", 0x46db, 0x00b2, 6},  // Jason
    ExeSpan{"name.1", 0x46db, 0x00b8, 4},
    ExeSpan{"name.2", 0x46db, 0x00bc, 7},
    ExeSpan{"name.3", 0x46db, 0x00c3, 5},
    ExeSpan{"name.4", 0x46db, 0x00c8, 5},
    ExeSpan{"name.5", 0x46db, 0x00cd, 5},
    ExeSpan{"name.6", 0x46db, 0x00d2, 7},
    ExeSpan{"name.7", 0x46db, 0x00d9, 6},
};

constexpr std::uint16_t kLoadSegment = 0x1000;

}  // namespace

std::span<const ExeSpan> OriginalExe::table() { return kDos103; }

OriginalExe::OriginalExe(std::span<const std::uint8_t> packed) {
    const std::string wanted =
        "this BTECH.EXE is not a version the rebuild knows (it supports " +
        std::string(kSupportedVersion) + ")";
    try {
        unpacked_ = unexepack(packed);
    } catch (const FormatError& e) {
        throw FormatError(wanted + ": " + e.what());
    }
    if (sha256_hex(unpacked_) != kSupportedUnpackedSha256) throw FormatError(wanted);
    image_start_ = std::size_t(unpacked_[8] | unpacked_[9] << 8) * 16;
}

OriginalExe OriginalExe::load(const std::filesystem::path& game_dir) {
    const auto path = find_game_file(game_dir, "BTECH.EXE");
    if (!path) throw FormatError("BTECH.EXE not found in " + game_dir.string());
    return OriginalExe(read_file(*path));
}

std::span<const std::uint8_t> OriginalExe::get(std::string_view name) const {
    const auto it = std::find_if(kDos103.begin(), kDos103.end(),
                                 [&](const ExeSpan& s) { return s.name == name; });
    if (it == kDos103.end()) throw FormatError("no piece of BTECH.EXE called " + std::string(name));
    const std::size_t at = image_start_ + std::size_t(it->segment - kLoadSegment) * 16 + it->offset;
    if (at + it->size > unpacked_.size()) throw FormatError("BTECH.EXE: piece out of range");
    return std::span(unpacked_).subspan(at, it->size);
}

std::string OriginalExe::text(std::string_view name) const {
    const auto bytes = get(name);
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t(0));
    return std::string(bytes.begin(), end);
}

}  // namespace chr::formats
