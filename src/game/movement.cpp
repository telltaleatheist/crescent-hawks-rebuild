#include "game/movement.h"

namespace chr::game {

using formats::MapPosition;

std::optional<Step> step_for_key(int k) {
    Step s;
    if (k == key::kUp || k == key::kPageUp || k == key::kHome) s.dy = -1;
    if (k == key::kDown || k == key::kPageDown || k == key::kEnd) s.dy = 1;
    if (k == key::kRight || k == key::kPageUp || k == key::kPageDown) s.dx = 1;
    if (k == key::kLeft || k == key::kHome || k == key::kEnd) s.dx = -1;
    if (s.dx == 0 && s.dy == 0) return std::nullopt;
    return s;
}

int blocking_tile(int map_number) { return map_number == 14 ? 0x21 : 0x55; }

namespace {

// Is the map's cell under this half-cell position inside the map?
bool inside(const formats::MtpMap& map, int cell_x, int cell_y) {
    return cell_x >= 0 && cell_y >= 0 && cell_x < map.width && cell_y < map.height;
}

// BTECH.EXE 287f:158c / 287f:163b / 287f:16e3 / 287f:17c5 (Ghidra FUN_287f_158c etc.)
// One half-cell up, down, left or right. Past the edge of a 64x64-cell area
// the position moves into the next area (the high byte's area number) and the
// program shifts its area grids and loads the maps there; at the edge of the
// 16x16-area world it does not move.
std::uint16_t move_x(std::uint16_t x, int dx) {
    const int low = (x & 0xff) + dx, area = x >> 8;
    if (low < 0) return area == 0 ? x : std::uint16_t((area - 1) << 8 | 0x7f);
    if (low > 0x7f) return area == 0x0f ? x : std::uint16_t((area + 1) << 8);
    return std::uint16_t((x & 0xff00) | low);
}
std::uint16_t move_y(std::uint16_t y, int dy) {
    const int low = (y & 0xff) + dy, area = y >> 8;
    if (low < 0) return area == 0 ? y : std::uint16_t((area - 0x10) << 8 | 0x7f);
    if (low > 0x7f) return area == 0xf0 ? y : std::uint16_t((area + 0x10) << 8);
    return std::uint16_t((y & 0xff00) | low);
}

}  // namespace

StepResult take_step(const formats::MtpMap& map, int map_number, MapPosition from,
                     const Party& party, Step step, EntranceQuestion& question,
                     const formats::Tables& tables) {
    StepResult result;
    result.position = from;
    const int hx = from.half_cell_x(), hy = from.half_cell_y();

    // Crossing into the next 8x8 block (16 half-cells) on either axis looks at
    // that block first: the block in the direction of the step, the diagonal
    // one when both axes cross (table DGROUP:04a0 into the 3x3 blocks at
    // 2c6c:07a4). A terrain block of type 0x0f is deep water.
    int bdx = 0, bdy = 0;
    if (step.dy < 0 && (hy & 0xf) == 0) bdy = -1;
    if (step.dy > 0 && (hy & 0xf) == 0xf) bdy = 1;
    if (step.dx < 0 && (hx & 0xf) == 0) bdx = -1;
    if (step.dx > 0 && (hx & 0xf) == 0xf) bdx = 1;
    if (bdx != 0 || bdy != 0) {
        const int bx = (hx >> 4) + bdx, by = (hy >> 4) + bdy;
        // Inside the map every block is one of the map's own (never water);
        // outside it the world's terrain decides (docs/movement.md).
        if (!inside(map, bx * 8, by * 8)) {
            result.outcome = StepOutcome::LeftMap;
            return result;
        }
    }

    // Each character in turn: first an entrance under its new position, then
    // the tile under its feet. A figure stands on half-cell (x + 1, y): the
    // cell checked is the one holding that half-cell.
    bool asked = false;  // F:d55c: one question per turn
    const int limit = blocking_tile(map_number);
    for (int i = 0; i < 8; ++i) {
        if (!party.present[i]) continue;
        const int fx = tables.formation_x[std::size_t(i)] + step.dx, fy = tables.formation_y[std::size_t(i)] + step.dy;
        if (!asked && map_number != 14) {  // ('Mech mode, map 14, has tile events instead)
            const std::uint16_t x = std::uint16_t(from.x + fx), y = std::uint16_t(from.y + fy);
            for (int e = 0; e < 12; ++e) {
                if (map.entrances[e].x != x || map.entrances[e].y != y) continue;
                asked = true;
                result.offered_place = e;
                result.entered = question.ask(e);
                break;
            }
        }
        const int cx = (hx + fx + 1) >> 1, cy = (hy + fy) >> 1;
        if (hx + fx + 1 < 0 || hy + fy < 0 || !inside(map, cx, cy)) {
            result.outcome = StepOutcome::LeftMap;
            return result;
        }
        if (map.cell(cx, cy) >= limit) {
            result.outcome = StepOutcome::Blocked;
            return result;
        }
    }

    // 1000:218f moves up or down first, then left or right.
    result.position.y = move_y(from.y, step.dy);
    result.position.x = move_x(from.x, step.dx);
    result.outcome = StepOutcome::Moved;
    return result;
}

}  // namespace chr::game
