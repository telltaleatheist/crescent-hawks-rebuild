#include "formats/exe_data.h"

#include <algorithm>
#include <array>
#include <cstdio>

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
    ExeSpan{ExeSpan::Kind::Font, "font", 0x2c6c, 0xa661, 0x400},
    // Text the game shows. The names are the text IDs (docs/text.md): a
    // text provider hands them out, and an override file can replace them.
    // 1000:1c12, the step check:
    ExeSpan{ExeSpan::Kind::Text, "walk.too_deep", 0x46db, 0x0410, 32},
    ExeSpan{ExeSpan::Kind::Text, "walk.enter_question", 0x46db, 0x0430, 20},  // then the place's name
    ExeSpan{ExeSpan::Kind::Text, "walk.enter_question_end", 0x46db, 0x0444, 2},
    // 1000:00bb, the main loop: after a walking key it writes the direction
    // in the upper panel box (pointers at DGROUP:01aa, in the order of the
    // keys at DGROUP:0160). Each starts with a new line.
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.north", 0x46db, 0x006c, 7},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.northeast", 0x46db, 0x0073, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.east", 0x46db, 0x007e, 6},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.southeast", 0x46db, 0x0084, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.south", 0x46db, 0x008f, 7},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.southwest", 0x46db, 0x0096, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.west", 0x46db, 0x00a1, 6},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.northwest", 0x46db, 0x00a7, 11},
    // 1000:1a13, the yes/no answer line, with Yes or with No picked:
    ExeSpan{ExeSpan::Kind::Text, "ui.answer_yes_picked", 0x46db, 0x03ee, 16},
    ExeSpan{ExeSpan::Kind::Text, "ui.answer_no_picked", 0x46db, 0x03fe, 17},
    // 1000:4cac, the heading over the party box's three bar columns:
    ExeSpan{ExeSpan::Kind::Text, "panel.party_heading", 0x46db, 0x0aca, 4},
    // The eight characters' names (pointers at DGROUP:01ca):
    ExeSpan{ExeSpan::Kind::Text, "character.0", 0x46db, 0x00b2, 6},
    ExeSpan{ExeSpan::Kind::Text, "character.1", 0x46db, 0x00b8, 4},
    ExeSpan{ExeSpan::Kind::Text, "character.2", 0x46db, 0x00bc, 7},
    ExeSpan{ExeSpan::Kind::Text, "character.3", 0x46db, 0x00c3, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.4", 0x46db, 0x00c8, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.5", 0x46db, 0x00cd, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.6", 0x46db, 0x00d2, 7},
    ExeSpan{ExeSpan::Kind::Text, "character.7", 0x46db, 0x00d9, 6},
};

constexpr std::uint16_t kLoadSegment = 0x1000;

}  // namespace

std::span<const ExeSpan> OriginalExe::table() { return kDos103; }

std::string OriginalExe::address(const ExeSpan& span) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04x:%04x", span.segment, span.offset);
    return buf;
}

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
    std::size_t n = 0;
    while (n < bytes.size()) {
        const std::uint8_t c = bytes[n];
        if (c == 0) break;
        ++n;
        if (c & 0x80) break;
        if (c == 0x02 || c == 0x06 || c == 0x09 || c == 0x13) ++n;  // and its argument
    }
    return std::string(bytes.begin(), bytes.begin() + std::ptrdiff_t(std::min(n, bytes.size())));
}

}  // namespace chr::formats
