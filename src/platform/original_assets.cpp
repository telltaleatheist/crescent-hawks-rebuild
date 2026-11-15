#include "platform/original_assets.h"

#include <cctype>
#include <exception>

#include "formats/cmp.h"
#include "formats/game_files.h"
#include "formats/palette.h"

namespace chr::platform {

namespace {

bool has_extension(const std::string& name, const char* ext) {
    const auto dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string e = name.substr(dot);
    for (auto& ch : e) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return e == ext;
}

Image from_colour_numbers(const std::vector<std::uint8_t>& numbers, int width, int height,
                          const formats::Palette16& palette) {
    Image image{width, height, width, height, {}};
    image.rgba.resize(numbers.size() * 4);
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        const auto& c = palette[numbers[i] & 0x0f];
        image.rgba[4 * i + 0] = c.r;
        image.rgba[4 * i + 1] = c.g;
        image.rgba[4 * i + 2] = c.b;
        image.rgba[4 * i + 3] = 0xff;
    }
    return image;
}

}  // namespace

OriginalAssets::OriginalAssets(std::filesystem::path game_dir) : game_dir_(std::move(game_dir)) {}

std::optional<Image> OriginalAssets::image(const AssetId& id) {
    last_error_.clear();
    try {
        const auto path = formats::find_game_file(game_dir_, id.file);
        if (!path) {
            last_error_ = id.file + " not found in " + game_dir_.string();
            return std::nullopt;
        }
        if (has_extension(id.file, ".CMP")) {
            if (id.index != 0) {
                last_error_ = to_string(id) + ": a .CMP holds one picture";
                return std::nullopt;
            }
            const auto picture = formats::decode_cmp(formats::read_file(*path));
            // The 256K EGA/VGA palette the program sets for its title screens.
            // Later the palette becomes part of what the game asks for (each
            // screen sets its own), not a fixed choice here.
            return from_colour_numbers(picture.pixels(), formats::kCmpWidth, formats::kCmpHeight,
                                       formats::title_palette_ega());
        }
        last_error_ = to_string(id) + ": no reader for this file type yet";
    } catch (const std::exception& e) {
        last_error_ = to_string(id) + ": " + e.what();
    }
    return std::nullopt;
}

}  // namespace chr::platform
