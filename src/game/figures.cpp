#include "game/figures.h"

#include "game/map_view.h"

namespace chr::game {

namespace {

// The walking scripts (37e8:0270..02ef) were part of BTECH.EXE; they are
// now imported into tables.json (formats/tables.h, walking_scripts). A byte
// 0-127 is a frame; 0xfd n records direction n; 0xfe n jumps back n bytes
// (counted from the n); 0xff steps back one byte (a frame held for ever).
// One script per direction, for the 'Mechs (0x270 + 8d) and for the
// characters (0x2b0 + 8d).
constexpr int kScriptBase = 0x270;
constexpr int character_script(int direction) { return 0x2b0 + 8 * direction; }  // DGROUP:027a

// F:d562: each character's first sprite; Jason's from the new-game setup
// at 1000:4f0f (docs/town_loop.md).
constexpr std::array<int, 8> kSpriteBase = {0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96};

}  // namespace

FigureAnimation::FigureAnimation(const formats::Tables& tables) : tables_(&tables) {
    for (int f = 0; f < kFigures; ++f) {
        pos_[f] = character_script(4) - kScriptBase;  // DGROUP:01f6: 37e8:02d0 for all
        direction_[f] = -1;                              // F:396c = 0xff
        frame_[f] = (f < 4 || (f >= 12 && f < 16)) ? 0 : 0x10;  // F:409a
    }
}

int FigureAnimation::step(int f) {
    for (;;) {
        const auto& scripts = tables_->walking_scripts;
        const auto b = std::int8_t(scripts[std::size_t(pos_[f]++)]);
        if (b >= 0) return b;
        if (b == -3) direction_[f] = scripts[std::size_t(pos_[f]++)];
        else if (b == -2) pos_[f] -= scripts[std::size_t(pos_[f])];
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
        // DGROUP:0160, the walking keys (imported: tables.json, walking_keys).
        if (tables_->walking_keys[std::size_t(d)] != k) continue;
        for (int f = 4; f < 12; ++f) walk(f, d);
        return;
    }
}

std::vector<Figure> party_figures(const formats::MtpMap& map, formats::MapPosition position,
                                  const Party& party, const FigureAnimation& animation,
                                  const formats::Tables& tables) {
    std::vector<Figure> figures;
    const int hx = position.half_cell_x(), hy = position.half_cell_y();
    const int parity = (position.y & 1) * 2 + (position.x & 1);
    const int map_left = map.block_x * formats::kBlockCells, map_top = map.block_y * formats::kBlockCells;
    int k = 0;
    for (int member = 0; member < 8; ++member) {
        if (!party.present[member]) continue;
        Figure f;
        f.sprite = sprite(kSpriteBase[member] + animation.character_frame(member));
        // DGROUP:02ae/02be put the k-th figure at the k-th formation offset
        // (DGROUP:0464/047c, imported: formation_x/y), one half-cell right of it.
        f.half_x = hx + tables.formation_x[std::size_t(k)] + 1;
        f.half_y = hy + tables.formation_y[std::size_t(k)];
        // DGROUP:02fe/030e/031e pick the cell under it in the 24x24-cell
        // window at 2c6c:07ad: the one holding that half-cell.
        const int cx = (f.half_x >> 1) - map_left, cy = (f.half_y >> 1) - map_top;
        if (cx >= 0 && cy >= 0 && cx < map.width && cy < map.height) {
            const int tile = map.cell(cx, cy);
            // DGROUP:0332 (imported: party_water): the water quarter bits.
            if (tile < 0xf6 && (tile & tables.party_water[std::size_t(k)][std::size_t(parity)]) != 0 && (tile & 0xf0) < 0x30)
                f.sink = (tile & 0xf0) == 0x20 ? 4 : 2;
        }
        figures.push_back(f);
        ++k;
    }
    return figures;
}

}  // namespace chr::game
