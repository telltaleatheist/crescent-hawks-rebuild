#include "game/terrain.h"

#include <utility>
#include <vector>

#include "formats/tables.h"
#include "game/random.h"

namespace chr::game {

namespace {

// The generator's lattice: 9x9 heights, an area's 8x8 blocks plus the
// next area's first row and column. kEmpty marks a point not yet made.
constexpr int kSide = 9;
constexpr std::uint8_t kEmpty = 0xff;
using Lattice = std::array<std::uint8_t, kSide * kSide>;

// What `cells` lattice cells of distance add at a midpoint: the noise byte
// masked to 0..4 cells - 1, less 2 cells; an 8-bit sum, and anything at
// 0x80 or more (a sum past the top or below zero) is 0.
std::uint8_t displaced(int average, std::uint8_t noise, int cells) {
    const std::uint8_t delta = std::uint8_t((noise & std::uint8_t(4 * cells - 1)) - 2 * cells);
    const std::uint8_t h = std::uint8_t(average + delta);
    return h >= 0x80 ? 0 : h;
}

// The noise table's next byte; the index is 8 bits and wraps.
struct Noise {
    const std::array<std::uint8_t, 256>& table;
    std::uint8_t index;
    std::uint8_t next() { return table[index++]; }
};

// BTECH.EXE 287f:0d07 (Ghidra FUN_287f_0d07), 287f:0d79 (FUN_287f_0d79)
// One edge, points a and b of the lattice `step` apart per cell: halved
// again and again, each new midpoint the average of its ends displaced by
// the noise. The halves wait on a stack, the second half first.
void subdivide_edge(Lattice& h, Noise& noise, int a, int b, int step) {
    std::vector<std::pair<int, int>> stack{{a, b}};
    while (!stack.empty()) {
        const auto [lo, hi] = stack.back();
        stack.pop_back();
        if (hi - lo == step) continue;
        const int mid = lo + (hi - lo) / 2;
        stack.push_back({lo, mid});
        stack.push_back({mid, hi});
        if (h[std::size_t(mid)] != kEmpty) continue;
        const int cells = (mid - lo) / step;
        h[std::size_t(mid)] = displaced((h[std::size_t(lo)] + h[std::size_t(hi)]) >> 1, noise.next(), cells);
    }
}

// BTECH.EXE 287f:0df8 (Ghidra FUN_287f_0df8)
// The inside: each square gets its centre (the average of its corners, no
// noise), then the midpoints of its top, left, bottom and right sides that
// are still empty (each its ends' average displaced), then its four
// quarters, the last quarter first.
void subdivide_inside(Lattice& h, Noise& noise, bool fix_noise) {
    struct Square { int nw, ne, sw, se; };
    std::vector<Square> stack{{0, kSide - 1, kSide * (kSide - 1), kSide * kSide - 1}};
    const auto side = [&](int at, int a, int b, int cells) {
        if (h[std::size_t(at)] != kEmpty) return;
        h[std::size_t(at)] = displaced((h[std::size_t(a)] + h[std::size_t(b)]) >> 1, noise.next(), cells);
    };
    while (!stack.empty()) {
        const Square s = stack.back();
        stack.pop_back();
        if (s.ne - s.nw == 1) continue;
        const int centre = s.nw + (s.se - s.nw) / 2;
        if (h[std::size_t(centre)] == kEmpty)
            h[std::size_t(centre)] = std::uint8_t(
                (h[std::size_t(s.nw)] + h[std::size_t(s.se)] + h[std::size_t(s.ne)] + h[std::size_t(s.sw)]) >> 2);
        const int cells = (s.ne - s.nw) / 2;
        const int top = s.nw + cells, left = s.nw + cells * kSide;
        const int bottom = s.sw + cells, right = s.ne + cells * kSide;
        side(top, s.nw, s.ne, cells);
        side(left, s.nw, s.sw, cells);
        side(bottom, s.sw, s.se, cells);
        // Bug 37: the right side's noise is scaled by cells / 8, which is 0:
        // the whole noise byte is added.
        side(right, s.ne, s.se, fix_noise ? cells : cells >> 3);
        stack.push_back({s.nw, top, left, centre});
        stack.push_back({top, s.ne, centre, right});
        stack.push_back({left, centre, s.sw, bottom});
        stack.push_back({centre, right, bottom, s.se});
    }
}

}  // namespace

Terrain::Terrain(const formats::Tables& tables, const Fixes& fixes)
    : tables_(tables), fixes_(fixes), noise_(start_up_noise()) {}

std::array<std::uint8_t, 256> Terrain::start_up_noise() {
    Random random;
    std::array<std::uint8_t, 256> noise{};
    for (auto& n : noise) n = random.next();
    return noise;
}

int Terrain::height(int index) const {
    // Before the table the noise table's end, after it 18 bytes of 0 and
    // then the templates.
    if (index < 0) return noise_[std::size_t(256 + index)];
    if (index < 256) return tables_.world_heights[std::size_t(index)];
    if (index < 256 + 18) return 0;
    return tables_.terrain_templates[std::size_t(index - 256 - 18)];
}

std::array<std::uint8_t, Terrain::kGridBlocks> Terrain::area_types(int nw, int ne, int sw, int se) const {
    Lattice h;
    h.fill(kEmpty);
    const int last = kSide - 1;
    h[0] = std::uint8_t(nw);
    h[std::size_t(last)] = std::uint8_t(ne);
    h[std::size_t(kSide * last)] = std::uint8_t(sw);
    h[std::size_t(kSide * last + last)] = std::uint8_t(se);
    // Each edge's noise starts at the sum of its corners, so two areas
    // make the same points along the edge they share.
    const auto edge = [&](int a, int b, int step) {
        Noise noise{noise_, std::uint8_t(h[std::size_t(a)] + h[std::size_t(b)])};
        subdivide_edge(h, noise, a, b, step);
        return noise.index;
    };
    edge(0, last, 1);
    edge(kSide * last, kSide * last + last, 1);
    edge(0, kSide * last, kSide);
    const std::uint8_t index = edge(last, kSide * last + last, kSide);
    Noise noise{noise_, index};
    subdivide_inside(h, noise, fixes_.fix_terrain_noise);
    // The type is the height's top four bits; the ninth row and column
    // belong to the neighbours.
    std::array<std::uint8_t, kGridBlocks> types{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) types[std::size_t(8 * r + c)] = h[std::size_t(kSide * r + c)] & 0xf0;
    return types;
}

void Terrain::generate(int area) {
    // Grid k is area + 16 (k / 3 - 1) + (k % 3 - 1), its corners the heights
    // of it, the next area east, south and south-east; no wrapping at the
    // world's edges.
    for (int k = 0; k < kGrids; ++k) {
        const int a = area - 0x11 + 16 * (k / 3) + k % 3;
        const auto types = area_types(height(a), height(a + 1), height(a + 16), height(a + 17));
        std::copy(types.begin(), types.end(), grids_.begin() + k * kGridBlocks);
    }
    link_blocks();
}

void Terrain::place_map(const formats::MtpMap& map, int grid) {
    int n = kFirstMapBlock;
    for (int r = 0; r < map.height / formats::kBlockCells; ++r)
        for (int c = 0; c < map.width / formats::kBlockCells; ++c)
            grids_[std::size_t(grid * kGridBlocks + (map.block_y + r) * 8 + map.block_x + c)] = std::uint8_t(n++);
}

void Terrain::link_blocks() {
    // The grids sit side by side as the 24x24 blocks around the party's area.
    const auto entry = [&](int bx, int by) -> int {
        if (bx < 0 || by < 0 || bx >= 24 || by >= 24) return -1;
        return grids_[std::size_t(((by / 8) * 3 + bx / 8) * kGridBlocks + (by % 8) * 8 + bx % 8)];
    };
    for (int k = 0; k < kGrids; ++k)
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) {
                const int bx = (k % 3) * 8 + c, by = (k / 3) * 8 + r;
                const int e = entry(bx, by);
                auto& mask = masks_[std::size_t(k * kGridBlocks + 8 * r + c)];
                // A map block's mask is its number in the block store, except
                // that block 0x80 on a grid's top or bottom row (not a corner)
                // is compared like a terrain type.
                const bool middle_of_top_or_bottom = (r == 0 || r == 7) && c > 0 && c < 7;
                if (e >= 0x80 && !(e == 0x80 && middle_of_top_or_bottom)) {
                    mask = std::uint8_t(e - 0x80);
                    continue;
                }
                mask = 0;
                if (entry(bx, by - 1) == e) mask |= 1;  // north
                if (entry(bx + 1, by) == e) mask |= 2;  // east
                if (entry(bx, by + 1) == e) mask |= 4;  // south
                if (entry(bx - 1, by) == e) mask |= 8;  // west
            }
}

}  // namespace chr::game
