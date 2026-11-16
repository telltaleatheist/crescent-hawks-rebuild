// The import table: the data the rebuild takes from the player's own
// BTECH.EXE (the font, and the game's text and tables that live in the
// program rather than in a data file), each piece by name with its place in
// the unpacked program. Only the importer (formats/import.h) reads the
// program; the game reads what it wrote. We never ship any of it
// (docs/rules.md, rule 1). The program is checked by its SHA-256 first, so a
// different version fails cleanly instead of handing back the wrong bytes.
// Other versions are added on purpose, when someone has one, with their own
// table.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace chr::formats {

// One named piece of the program: segment:offset as in docs/functions.md
// (load segment 0x1000) and its length in bytes.
struct ExeSpan {
    enum class Kind { Font, Text, Table };
    Kind kind;
    std::string_view name;  // for text, the text ID (docs/text.md)
    std::uint16_t segment;
    std::uint16_t offset;
    std::uint16_t size;
};

// The version we support: DOS 1.03, unpacked SHA-256 below.
inline constexpr std::string_view kSupportedVersion = "DOS 1.03";
inline constexpr std::string_view kSupportedUnpackedSha256 =
    "056b40670a95c4a4e56f7dda405dc0e2f3c98fcb530ed72f476d374051abd16d";

class OriginalExe {
public:
    // Unpacks and checks `packed` (the BTECH.EXE file). Throws FormatError
    // naming the supported version if it is not that program.
    explicit OriginalExe(std::span<const std::uint8_t> packed);
    static OriginalExe load(const std::filesystem::path& game_dir);

    // Where a piece is, "ssss:oooo", for the imported files.
    static std::string address(const ExeSpan& span);

    // The bytes of the named piece. Throws FormatError for an unknown name.
    std::span<const std::uint8_t> get(std::string_view name) const;
    // A named piece as text, read the way the printer (2656:03f5) reads it:
    // up to the NUL that ends it, where the codes 02, 06, 09 and 13 take the
    // next byte as their argument (so `06 00`, black, is not the end), and a
    // byte with the top bit set is the last one.
    std::string text(std::string_view name) const;
    // The same reading of any bytes (for texts inside a table).
    static std::string text_of(std::span<const std::uint8_t> bytes);

    static std::span<const ExeSpan> table();

private:
    std::vector<std::uint8_t> unpacked_;
    std::size_t image_start_ = 0;  // where the load image starts in unpacked_
};

}  // namespace chr::formats
