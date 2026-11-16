#include "formats/font_file.h"

#include <cstdio>
#include <sstream>

#include "formats/cmp.h"  // FormatError

namespace chr::formats {

Font font_from_original(std::span<const std::uint8_t> bits) {
    if (bits.size() != 128 * 8) throw FormatError("the original font is 1,024 bytes");
    Font font{8, 8, {}};
    for (int ch = 0; ch < 128; ++ch) {
        std::vector<std::uint8_t> g(64);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) g[y * 8 + x] = (bits[std::size_t(ch) * 8 + y] >> (7 - x)) & 1;
        font.glyphs.push_back(std::move(g));
    }
    return font;
}

std::string write_font_file(const Font& font) {
    std::string out = "chr-font 1\nsize " + std::to_string(font.width) + " " + std::to_string(font.height) + "\n";
    for (std::size_t ch = 0; ch < font.glyphs.size(); ++ch) {
        out += "glyph " + std::to_string(ch) + "\n";
        for (int y = 0; y < font.height; ++y) {
            for (int x = 0; x < font.width; ++x) out += font.glyphs[ch][std::size_t(y * font.width + x)] ? '#' : '.';
            out += '\n';
        }
    }
    return out;
}

Font parse_font_file(std::string_view text) {
    std::istringstream in{std::string(text)};
    std::string line;
    int number = 0;
    auto next = [&]() -> bool {
        while (std::getline(in, line)) {
            ++number;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) return true;
        }
        return false;
    };
    auto bad = [&](const std::string& why) {
        return FormatError("font file, line " + std::to_string(number) + ": " + why);
    };
    if (!next() || line != "chr-font 1") throw bad("expected \"chr-font 1\"");
    Font font;
    if (!next() || std::sscanf(line.c_str(), "size %d %d", &font.width, &font.height) != 2 ||
        font.width < 1 || font.height < 1 || font.width > 64 || font.height > 64)
        throw bad("expected \"size W H\"");
    font.glyphs.assign(128, std::vector<std::uint8_t>(std::size_t(font.width * font.height), 0));
    while (next()) {
        int ch = -1;
        if (std::sscanf(line.c_str(), "glyph %d", &ch) != 1 || ch < 0 || ch > 127)
            throw bad("expected \"glyph N\" (0-127)");
        auto& g = font.glyphs[std::size_t(ch)];
        for (int y = 0; y < font.height; ++y) {
            if (!next() || int(line.size()) != font.width) throw bad("expected a row of " + std::to_string(font.width));
            for (int x = 0; x < font.width; ++x) {
                if (line[std::size_t(x)] != '#' && line[std::size_t(x)] != '.') throw bad("rows use # and .");
                g[std::size_t(y * font.width + x)] = line[std::size_t(x)] == '#';
            }
        }
    }
    return font;
}

}  // namespace chr::formats
