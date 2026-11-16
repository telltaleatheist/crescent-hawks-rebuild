#include "game/townspeople.h"

#include "game/movement.h"
#include "game/world.h"

namespace chr::game {

namespace {

using formats::MapPosition;

// Figure numbers: the townspeople are 16-23.
constexpr int kFirstWalker = 16;

// F:d56e..d575: the townspeople's first sprite (0xfe for all: the grey
// people of MECHSHAP.CMP's row 40, plus the walking frame 0x10-0x1f).
constexpr int kWalkerSprite = 0xfe;

// P:0240 (Ghidra 2c6c:0240): direction_toward's answer for each combination of its four
// tests (north 8, south 4, east 2, west 1).
constexpr std::array<int, 16> kDirectionOf = {-1, 6, 2, -1, 4, 5, 3, -1, 0, 7, 1, -1, -1, -1, -1, -1};

// DGROUP:311a/312a: one half-cell in each direction.
constexpr std::array<int, 8> kStepX = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr std::array<int, 8> kStepY = {-1, -1, 0, 1, 1, 1, 0, -1};

// DGROUP:310a: the turns tried, read from the end: straight on, then one
// way and the other, wider and wider. Two patterns, mirrored.
constexpr std::array<std::int8_t, 16> kTurns = {7, -6, 5, -4, 3, -2, 1, 0, -7, 6, -5, 4, -3, 2, -1, 0};

// DGROUP:032e: for the person's odd/even half-cells, which quarter bits of
// the tile under it mean water there.
constexpr std::array<std::uint8_t, 4> kWaterQuarters = {0x9, 0x3, 0xc, 0x6};

std::int16_t wrap16(int v) { return std::int16_t(std::uint16_t(v)); }

// The tile of the map cell holding half-cell (x, y), or none off the map.
// (The original reads a 24x24-cell window around the party at 2c6c:07ad.)
int tile_at(const formats::MtpMap& map, MapPosition p) {
    const int cx = ((p.half_cell_x() + 1) >> 1) - map.block_x * formats::kBlockCells;
    const int cy = (p.half_cell_y() >> 1) - map.block_y * formats::kBlockCells;
    if (cx < 0 || cy < 0 || cx >= map.width || cy >= map.height) return -1;
    return map.cell(cx, cy);
}

}  // namespace

// BTECH.EXE 1e31:023c..02a6, the tile a person would step onto, as the
// original finds it: not in the map but in the 24x24-cell window that
// 287f:1314 fills with the 3x3 blocks around the party's block (P:07ad), at
// the view's corner cell P:09ed plus the place relative to the view
// (1000:2689: x - party x + 0x1a, y - party y + 0x0c, across area edges as
// the original). Rows are 24 cells, so a place past the window's right or
// left edge reads the next or the previous row's cell instead: a person
// far to the side of the party can be stopped by a cell that is not where
// it steps (docs/townspeople.md). An index outside the window would read
// whatever follows or precedes it in memory; the rebuild reads the real
// cell there and says so in `outside`.
int window_tile(const formats::MtpMap& map, MapPosition party, MapPosition next, bool& outside) {
    int rel_x = wrap16(next.x - party.x);
    if (rel_x < -0x80) rel_x += 0x80;
    if (rel_x > 0x80) rel_x -= 0x80;
    rel_x += 0x1a;
    int rel_y = wrap16(next.y - party.y);
    if (rel_y < wrap16(0xf080)) rel_y += 0xf80;
    if (rel_y > 0xf80) rel_y -= 0xf80;
    rel_y += 0x0c;
    auto floor_half = [](int v) { return v >= 0 ? v / 2 : -((1 - v) / 2); };  // sar 1
    int index = floor_half(rel_y) * 24 + floor_half(rel_x - 13);
    if ((rel_x & 1) == 0 && (party.x & 1) != 0) ++index;
    if ((rel_y & 1) != 0 && (party.y & 1) != 0) index += 24;
    // P:09ed: where the view's corner cell is in the window.
    index += (((party.half_cell_y() >> 1) & 7) + 2) * 24 + ((party.half_cell_x() >> 1) & 7) + 2;
    outside = index < 0 || index >= 24 * 24;
    if (outside) return tile_at(map, next);
    const int cx = ((party.half_cell_x() >> 4) - 1) * formats::kBlockCells + index % 24 -
                   map.block_x * formats::kBlockCells;
    const int cy = ((party.half_cell_y() >> 4) - 1) * formats::kBlockCells + index / 24 -
                   map.block_y * formats::kBlockCells;
    if (cx < 0 || cy < 0 || cx >= map.width || cy >= map.height) return -1;
    return map.cell(cx, cy);
}

int direction_toward(MapPosition from, MapPosition to) {
    // Rows: from - to, in 8 bits; across an area edge only the sign is kept.
    int dy = std::int8_t(std::uint8_t(from.y - to.y));
    const int from_area_y = (from.y >> 8) & 0xf0, to_area_y = (to.y >> 8) & 0xf0;
    if (to_area_y > from_area_y) dy = wrap16(dy | 0xff80);
    else if (to_area_y < from_area_y) dy &= 0x7f;
    // Columns: to - from in 16 bits; across an area edge likewise.
    int dx = wrap16(to.x - from.x);
    const int from_hi = from.x >> 8, to_hi = to.x >> 8;
    if (to_hi > from_hi) dx &= 0x7f;
    else if (to_hi < from_hi) dx = wrap16(dx | 0x80);
    int bits = 0;
    const int adx = wrap16(dx < 0 ? -dx : dx), ady = wrap16(dy < 0 ? -dy : dy);
    if (wrap16(dy * 2) >= adx) bits |= 8;
    if (wrap16(-dy * 2) >= adx) bits |= 4;
    if (wrap16(dx * 2) >= ady) bits |= 2;
    if (wrap16(-dx * 2) >= ady) bits |= 1;
    return kDirectionOf[std::size_t(bits)];
}

Townspeople Townspeople::place(const formats::MtpMap& map, int map_number, Random& random) {
    Townspeople t;
    t.blocking_ = blocking_tile(map_number);
    for (int i = 0; i < 8; ++i) {
        auto& w = t.walkers_[std::size_t(i)];
        const int r = random.next() & 7;
        const int next = map.start_shuffle[std::size_t(r)] & 0x0f;
        w.wait = 1;
        w.route = std::uint8_t(r << 4 | next);
        w.target = map.entrances[std::size_t(next)];
        w.at = w.target;
        w.heading = 0;
        if (map_number > 10) {
            w.wait = 0;
            w.at = {};
        }
    }
    return t;
}

void Townspeople::move(const formats::MtpMap& map, MapPosition party, const PartyPlaces& party_places,
                       FigureAnimation& animation, Random& random) {
    for (int i = 0; i < 8; ++i) {
        auto& w = walkers_[std::size_t(i)];
        const int f = kFirstWalker + i;
        if (w.wait != 0) {
            // (F:d339 keeps the first person inside: not set in town.)
            if (--w.wait == 0) w.at = map.entrances[std::size_t(w.route >> 4)];
            continue;
        }
        // Only near the party: inside its 16x16 square of half-cells
        // (x & 0xf70) widened by 16 each way, compared as plain numbers.
        const std::uint16_t left = move_x(std::uint16_t(party.x & 0x0f70), -16);
        const std::uint16_t right = move_x(std::uint16_t(party.x | 0x0f), 16);
        const std::uint16_t top = move_y(std::uint16_t(party.y & 0xf070), -16);
        const std::uint16_t bottom = move_y(std::uint16_t(party.y | 0x0f), 16);
        if (!(w.at.x > left && w.at.x < right && w.at.y > top && w.at.y < bottom)) continue;

        step(i, map, party, party_places);
        animation.walk(f, w.heading);
        if (w.at == w.target) {
            // In through the door: out again after (random & 31) rounds.
            w.wait = std::uint8_t(random.next() & 0x1f);
            if (w.wait == 0 && fix_lost_walker) w.wait = 1;
            w.at = {};
            const int next = map.start_shuffle[std::size_t(w.route & 7)] & 0x0f;
            w.route = std::uint8_t(w.route << 4 | next);
            w.target = map.entrances[std::size_t(next)];
        }
    }
}

// BTECH.EXE 1e31:0006 (Ghidra FUN_1e31_0006), for a person (it also moves
// the 'Mechs, which need a second free cell beside the first, and others:
// not rebuilt).
// Turns one eighth toward the target (unless already facing it), then tries
// eight directions, turning by kTurns from the end each time (straight on
// first); takes the first one onto a cell that does not block (tile below
// DGROUP:0150, read through the cell window: window_tile) and that no other
// figure stands on. Stays put if at the
// target already or if every way is shut.
void Townspeople::step(int i, const formats::MtpMap& map, MapPosition party, const PartyPlaces& party_places) {
    auto& w = walkers_[std::size_t(i)];
    if (--wobble_count_ == 0) {
        wobble_count_ = 30;
        wobble_ ^= 8;
    }
    const int want = direction_toward(w.at, w.target);
    if (want == -1) return;
    int d = w.heading;
    if (want != d) d = (((want - d) & 7) < 5 ? d + 1 : d - 1) & 7;
    for (int k = 7; k >= 0; --k) {
        d = (d + kTurns[std::size_t(wobble_ + k)]) & 7;
        MapPosition next = w.at;
        if (kStepX[std::size_t(d)] != 0) next.x = step_x(next.x, kStepX[std::size_t(d)]);
        if (kStepY[std::size_t(d)] != 0) next.y = step_y(next.y, kStepY[std::size_t(d)]);
        bool outside = false;
        const int tile = window_tile(map, party, next, outside);
        if (outside) outside_window = true;
        if (tile < 0 || tile >= blocking_) continue;
        // BTECH.EXE 1e31:16ab (Ghidra FUN_1e31_16ab): a character of the
        // party where it was last drawn, or another person. (And a 'Mech's
        // three half-cells: none in town.)
        bool taken = false;
        for (const auto& place : party_places) taken = taken || place == next;
        for (std::size_t j = 0; j < walkers_.size(); ++j)
            taken = taken || (j != std::size_t(i) && walkers_[j].at == next);
        if (taken) {
            // F:d57e: someone stands on the target itself.
            if (next == w.target) target_taken = true;
            continue;
        }
        w.at = next;
        w.heading = std::int8_t(d);
        return;
    }
}

std::vector<Figure> Townspeople::figures(const formats::MtpMap& map, MapPosition party,
                                         const FigureAnimation& animation) const {
    std::vector<Figure> out;
    for (int i = 0; i < 8; ++i) {
        const auto& w = walkers_[std::size_t(i)];
        if (w.wait != 0) continue;
        // In view: within 13 half-cells across and 12 down of the party
        // (in its area; further areas by a wider test, as the original).
        const int rel_x = wrap16(w.at.x - party.x) + 0x1a;
        const int rel_y = wrap16(w.at.y - party.y) + 0x0c;
        bool hidden = false;
        if ((w.at.x & 0xff00) == (party.x & 0xff00) && (rel_x < 13 || rel_x > 39)) hidden = true;
        if ((w.at.y & 0xff00) == (party.y & 0xff00) && (rel_y < 0 || rel_y > 24)) hidden = true;
        if (rel_x < -0x73 || rel_x > 0xa7 || rel_y < -0xf80 || rel_y > 0xf98 || hidden) continue;
        Figure fig;
        fig.sprite = sprite(kWalkerSprite + animation.frame(kFirstWalker + i));
        // Drawn where the party's leader would be if it stood there.
        fig.half_x = w.at.half_cell_x() + 1;
        fig.half_y = w.at.half_cell_y();
        const int tile = tile_at(map, w.at);
        const int quarters = kWaterQuarters[std::size_t((w.at.y & 1) * 2 + ((w.at.x + 1) & 1))];
        if (tile >= 0 && tile < 0xf6 && (tile & quarters) != 0 && (tile & 0xf0) < 0x30)
            fig.sink = (tile & 0xf0) == 0x20 ? 4 : 2;
        out.push_back(fig);
    }
    return out;
}

}  // namespace chr::game
