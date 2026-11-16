// Data the rebuild reads from the player's own BTECH.EXE: the font, and the
// game's text and tables that live in the program rather than in a data file.
// We never ship any of it (docs/rules.md, rule 1). Each piece has a name and a
// fixed place in the unpacked program of one known version; the program is
// checked by its SHA-256 first, so a different version fails cleanly instead
// of handing back the wrong bytes. Other versions are added on purpose, when
// someone has one, with their own table.
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
    std::string_view name;
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

    // The bytes of the named piece. Throws FormatError for an unknown name.
    std::span<const std::uint8_t> get(std::string_view name) const;
    // A named piece as text: its bytes up to the first NUL, or all of them.
    std::string text(std::string_view name) const;

    static std::span<const ExeSpan> table();

private:
    std::vector<std::uint8_t> unpacked_;
    std::size_t image_start_ = 0;  // where the load image starts in unpacked_
};

}  // namespace chr::formats
