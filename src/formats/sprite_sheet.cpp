#include "formats/sprite_sheet.h"

#include <array>

namespace chr::formats {

namespace {

// 273d:070a takes the slot, a column of 8 pixels, a row in pixels, a width
// in 8-pixel columns and a height in pixels, and copies that rectangle of
// the unpacked picture into a sprite of its own (4-byte header: height - 1,
// width).
struct Cut {
    int col, y, cols, height;
};

std::array<std::optional<Cut>, kSpriteSlots> make_table() {
    std::array<std::optional<Cut>, kSpriteSlots> t{};
    auto cut = [&](int slot, int col, int y, int cols, int height) { t[std::size_t(slot)] = Cut{col, y, cols, height}; };
    // The order of 1527:0044.
    for (int i = 0; i < 12; ++i) cut(i, 3 * i, 0, 3, 24);           // 24x24 'Mechs
    for (int i = 12; i < 16; ++i) cut(i, 3 * i - 36, 24, 3, 24);
    for (int i = 0x10; i < 0x24; ++i) cut(i, i - 4, 24, 1, 8);       // 8x8 people
    for (int r = 9; r < 16; ++r)
        for (int c = 12; c < 24; ++c) cut(12 * r + c - 0x54, c, 8 * r, 1, 8);
    cut(0x78, 0, 0x60, 3, 24);
    cut(0x79, 3, 0x60, 3, 24);
    cut(0x7a, 9, 0x78, 3, 24);
    cut(0x7b, 6, 0x78, 3, 24);
    cut(0x7c, 8, 0xa0, 2, 14);
    cut(0x7d, 10, 0xa0, 2, 14);
    cut(0x7e, 0, 0xa0, 1, 8);
    cut(0x7f, 1, 0xa0, 1, 8);
    cut(0x80, 8, 0x90, 2, 16);
    cut(0x81, 10, 0x90, 2, 16);
    for (int r = 0x12; r < 0x16; ++r)
        for (int c = 4; c < 8; ++c) cut(4 * r + c + 0x36, c, 8 * r, 1, 8);
    for (int i = 0; i < 12; ++i) cut(0x92 + i, 3 * i, 0x30, 3, 24);
    for (int i = 0; i < 4; ++i) cut(0x9e + i, 3 * i, 0x48, 3, 24);
    cut(0xa2, 6, 0x60, 3, 24);
    cut(0xa3, 9, 0x60, 3, 24);
    cut(0xa4, 0, 0x78, 3, 24);
    cut(0xa5, 3, 0x78, 3, 24);
    for (int s = 0xa6; s < 0xba; ++s) cut(s, s - 0x9a, 0x20, 1, 8);  // 8x8 people
    for (int r = 9; r < 16; ++r)
        for (int c = 24; c < 36; ++c) cut(12 * r + c + 0x36, c, 8 * r, 1, 8);
    for (int n = 0x10a; n < 0x11e; ++n) cut(n + 4, n - 0xfe, 0x28, 1, 8);
    for (int r = 0x10; r < 0x17; ++r)
        for (int c = 12; c < 24; ++c) cut(12 * r + c + 0x56, c, 8 * r, 1, 8);
    cut(0x176, 0, 0x90, 2, 11);
    cut(0x177, 2, 0x90, 2, 11);
    return t;
}

}  // namespace

std::optional<SpriteRect> mechshap_sprite(int slot) {
    static const auto table = make_table();
    if (slot < 0 || slot >= kSpriteSlots || !table[std::size_t(slot)]) return std::nullopt;
    const auto& c = *table[std::size_t(slot)];
    return SpriteRect{c.col * 8, c.y, c.cols * 8, c.height};
}

}  // namespace chr::formats
