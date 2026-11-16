#include "platform/original_assets.h"

#include <algorithm>
#include <cctype>
#include <exception>

#include "formats/cells.h"
#include "formats/cmp.h"
#include "formats/game_files.h"
#include "formats/icn.h"
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

const formats::TileSheet* OriginalAssets::sheet(const std::string& file) {
    if (auto it = sheets_.find(file); it != sheets_.end()) return &it->second;
    const auto path = formats::find_game_file(game_dir_, file);
    if (!path) {
        last_error_ = file + " not found in " + game_dir_.string();
        return nullptr;
    }
    return &sheets_.emplace(file, formats::decode_icn(formats::read_file(*path))).first->second;
}

const formats::OriginalExe* OriginalAssets::exe() {
    if (!exe_ && exe_error_.empty()) {
        try {
            exe_ = std::make_unique<formats::OriginalExe>(formats::OriginalExe::load(game_dir_));
        } catch (const std::exception& e) {
            exe_error_ = e.what();
        }
    }
    if (!exe_) last_error_ = exe_error_;
    return exe_.get();
}

std::optional<Image> OriginalAssets::image(const AssetId& id) {
    last_error_.clear();
    try {
        if (id.file == "FONT") {
            const auto* program = exe();
            if (!program) return std::nullopt;
            const auto bits = formats::font_character(program->get("font"), id.index);
            Image glyph{8, 8, 8, 8, std::vector<std::uint8_t>(64 * 4, 0)};
            for (int i = 0; i < 64; ++i)
                if (bits[i]) std::fill_n(glyph.rgba.begin() + 4 * i, 4, std::uint8_t(0xff));
            return glyph;
        }
        if (const auto colon = id.file.find(":8x8"); colon != std::string::npos) {
            const std::string file = id.file.substr(0, colon);
            auto it = pictures_.find(file);
            if (it == pictures_.end()) {
                const auto path = formats::find_game_file(game_dir_, file);
                if (!path) {
                    last_error_ = file + " not found in " + game_dir_.string();
                    return std::nullopt;
                }
                it = pictures_.emplace(file, formats::decode_cmp(formats::read_file(*path))).first;
            }
            const auto cell = formats::picture_cell(it->second, id.index);
            return from_colour_numbers({cell.begin(), cell.end()}, 8, 8, formats::standard_palette_ega());
        }
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
            // The standard 256K EGA/VGA palette. Later the palette becomes part
            // of what the game asks for (some screens set their own), not a
            // fixed choice here.
            return from_colour_numbers(picture.pixels(), formats::kCmpWidth, formats::kCmpHeight,
                                       formats::standard_palette_ega());
        }
        if (has_extension(id.file, ".ICN")) {
            const auto* tiles = sheet(id.file);
            if (!tiles) return std::nullopt;
            const auto pixels = tiles->tile(id.index);
            return from_colour_numbers({pixels.begin(), pixels.end()}, formats::kTileSize,
                                       formats::kTileSize, formats::standard_palette_ega());
        }
        last_error_ = to_string(id) + ": no reader for this file type yet";
    } catch (const std::exception& e) {
        last_error_ = to_string(id) + ": " + e.what();
    }
    return std::nullopt;
}

}  // namespace chr::platform
