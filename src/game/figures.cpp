#include "game/figures.h"

#include "game/map_view.h"

namespace chr::game {

namespace {

// The walking scripts at 37e8:0270..02ef. A byte 0-127 is a frame; 0xfd n
// records direction n; 0xfe n jumps back n bytes (counted from the n); 0xff
// steps back one byte (a frame held for ever). One script per direction,
// for the 'Mechs (0x270 + 8d) and for the characters (0x2b0 + 8d).
constexpr int kScriptBase = 0x270;
constexpr std::array<std::uint8_t, 0x80> kScripts = {
    0xfd, 0, 0x00, 0x01, 0x02, 0x03, 0xfe, 5,  0xfd, 1, 0x04, 0x05, 0x06, 0x07, 0xfe, 5,
    0xfd, 2, 0x06, 0x07, 0x04, 0x05, 0xfe, 5,  0xfd, 3, 0x05, 0x06, 0x07, 0x04, 0xfe, 5,
    0xfd, 4, 0x08, 0x09, 0x0a, 0x0b, 0xfe, 5,  0xfd, 5, 0x0c, 0x0d, 0x0e, 0x0f, 0xfe, 5,
    0xfd, 6, 0x0f, 0x0c, 0x0d, 0x0e, 0xfe, 5,  0xfd, 7, 0x0e, 0x0f, 0x0c, 0x0d, 0xfe, 5,
    0xfd, 0, 0x18, 0x19, 0x1a, 0x1b, 0xfe, 5,  0xfd, 1, 0x14, 0x15, 0x16, 0x17, 0xfe, 5,
    0xfd, 2, 0x14, 0x15, 0x16, 0x17, 0xfe, 5,  0xfd, 3, 0x14, 0x15, 0x16, 0x17, 0xfe, 5,
    0xfd, 4, 0x10, 0x11, 0x12, 0x13, 0xfe, 5,  0xfd, 5, 0x1c, 0x1d, 0x1e, 0x1f, 0xfe, 5,
    0xfd, 6, 0x1c, 0x1d, 0x1e, 0x1f, 0xfe, 5,  0xfd, 7, 0x1c, 0x1d, 0x1e, 0x1f, 0xfe, 5,
};
constexpr int character_script(int direction) { return 0x2b0 + 8 * direction; }  // DGROUP:027a

// DGROUP:0160: the walking keys in direction order N, NE, E, SE, S, SW, W, NW.
constexpr std::array<int, 8> kDirectionKeys = {key::kUp,   key::kPageUp, key::kRight, key::kPageDown,
                                               key::kDown, key::kEnd,    key::kLeft,  key::kHome};

// F:d562: each character's first sprite. Only Jason's is known so far
// (0x96, set by the new-game setup at 1000:4f0f); the others are set when
// they join the party, not rebuilt yet.
constexpr std::array<int, 8> kSpriteBase = {0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96};

// DGROUP:0332: for the k-th figure drawn and the position's odd/even
// half-cells, which quarter bits of the tile under it mean water there.
constexpr std::array<std::array<std::uint8_t, 4>, 8> kWaterQuarters = {{
    {0x3, 0x9, 0x6, 0xc}, {0x6, 0xc, 0x3, 0x9}, {0x6, 0xc, 0x3, 0x9}, {0x9, 0x3, 0xc, 0x6},
    {0x9, 0x3, 0xc, 0x6}, {0xc, 0x6, 0x9, 0x3}, {0xc, 0x6, 0x9, 0x3}, {0xc, 0x6, 0x9, 0x3},
}};

}  // namespace

FigureAnimation::FigureAnimation() {
    for (int f = 0; f < kFigures; ++f) {
        pos_[f] = character_script(4) - kScriptBase;  // DGROUP:01f6: 37e8:02d0 for all
        direction_[f] = -1;                              // F:396c = 0xff
        frame_[f] = (f < 4 || (f >= 12 && f < 16)) ? 0 : 0x10;  // F:409a
    }
}

int FigureAnimation::step(int f) {
    for (;;) {
        const auto b = std::int8_t(kScripts[std::size_t(pos_[f]++)]);
        if (b >= 0) return b;
        if (b == -3) direction_[f] = kScripts[std::size_t(pos_[f]++)];
        else if (b == -2) pos_[f] -= kScripts[std::size_t(pos_[f])];
        else if (b == -1) pos_[f] -= 1;
    }
}

void FigureAnimation::start_script(int f, int address) { pos_[std::size_t(f)] = address - kScriptBase; }

void FigureAnimation::walk(int f, int direction) {
    if (direction_[f] != direction) pos_[f] = character_script(direction) - kScriptBase;
    frame_[f] = step(f);
}

void FigureAnimation::key(int k) {
    for (int d = 0; d < 8; ++d) {
        if (kDirectionKeys[d] != k) continue;
        for (int f = 4; f < 12; ++f) walk(f, d);
        return;
    }
}

std::vector<Figure> party_figures(const formats::MtpMap& map, formats::MapPosition position,
                                  const Party& party, const FigureAnimation& animation) {
    std::vector<Figure> figures;
    const int hx = position.half_cell_x(), hy = position.half_cell_y();
    const int parity = (position.y & 1) * 2 + (position.x & 1);
    const int map_left = map.block_x * formats::kBlockCells, map_top = map.block_y * formats::kBlockCells;
    int k = 0;
    for (int member = 0; member < 8; ++member) {
        if (!party.present[member]) continue;
        Figure f;
        f.sprite = sprite(kSpriteBase[member] + animation.character_frame(member));
        // DGROUP:02ae/02be put the k-th figure at the k-th formation offset,
        // one half-cell right of it (as the step check assumes).
        f.half_x = hx + kFormationX[k] + 1;
        f.half_y = hy + kFormationY[k];
        // DGROUP:02fe/030e/031e pick the cell under it in the 24x24-cell
        // window at 2c6c:07ad: the one holding that half-cell.
        const int cx = (f.half_x >> 1) - map_left, cy = (f.half_y >> 1) - map_top;
        if (cx >= 0 && cy >= 0 && cx < map.width && cy < map.height) {
            const int tile = map.cell(cx, cy);
            if (tile < 0xf6 && (tile & kWaterQuarters[k][parity]) != 0 && (tile & 0xf0) < 0x30)
                f.sink = (tile & 0xf0) == 0x20 ? 4 : 2;
        }
        figures.push_back(f);
        ++k;
    }
    return figures;
}

}  // namespace chr::game
