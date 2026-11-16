#include <cstdio>

#include "game/combat.h"
#include "game/fight.h"
#include "game/map_view.h"
#include "game/town.h"
#include "game/world.h"

namespace chr::game {

using formats::MapPosition;

namespace {

// A view-relative half-cell (x - party x + 0x1a, y - party y + 0x0c) back to
// a position, across area edges as 15ab:0d3d does it.
std::uint16_t view_x(std::uint16_t party, int rel) {
    std::uint16_t x = std::uint16_t(rel + party - 0x1a);
    if (x & 0x80) x = rel < 0x1a ? std::uint16_t(x & 0x0f7f) : std::uint16_t(x + 0x80);
    return x;
}
std::uint16_t view_y(std::uint16_t party, int rel) {
    std::uint16_t y = std::uint16_t(rel + party - 0x0c);
    if (y & 0x80) y = rel < 0x0c ? std::uint16_t(y & 0xf07f) : std::uint16_t(y + 0x0f80);
    return y;
}

}  // namespace

// BTECH.EXE 203b:000a (Ghidra FUN_203b_000a), a fight of kind 0: the random
// encounter outdoors, as far as its "no fight" ending (docs/combat.md 2.1-2.3,
// 2.12, 2.13). A fight that does take place is not rebuilt yet.
void Town::random_encounter() {
    const MapPosition at = position_;
    // 2.1: every figure's path ends at once, no targets, no orders; the
    // 'Mechs cool; the computer moves no party figure. The townspeople keep
    // their places in their records, and every figure is emptied, its
    // heading too (figures 16-23's are the townspeople's, F:3920 + 16 + i).
    for (int f = 0; f < CombatState::kFigures; ++f) {
        combat_.paths[std::size_t(f)].fill(2);
        combat_.targets[std::size_t(f)].fill(0xff);
        combat_.orders[std::size_t(f)].fill(0xff);
    }
    combat_.heat.fill(0);
    combat_.burning.fill(0);
    for (int f = 0; f < 12; ++f) figs_.computer[std::size_t(f)] = false;
    people_.leave_for_fight();
    townspeople_away_ = true;
    for (int f = 0; f < FightFigures::kCount; ++f) {
        figs_.destroyed[std::size_t(f)] = false;
        figs_.active[std::size_t(f)] = 0;
        figs_.heading[std::size_t(f)] = -1;
        figs_.x[std::size_t(f)] = figs_.y[std::size_t(f)] = 0xffff;
    }
    for (auto& w : people_.walkers()) w.heading = -1;
    // 2.2: the characters on foot at the formation's places, in turn, then
    // the 'Mechs at theirs; only those that exist count.
    int k = 0;
    for (int c = 0; c < 8; ++c) {
        if (!on_foot(c)) continue;
        const int f = 4 + c;
        figs_.x[std::size_t(f)] = move_x(at.x, tables_.foot_formation_x[std::size_t(k)]);
        figs_.y[std::size_t(f)] = move_y(at.y, tables_.foot_formation_y[std::size_t(k)]);
        ++figs_.active[std::size_t(f)];
        ++k;
    }
    k = 0;
    for (int m = 0; m < 4; ++m) {
        if (mechs_[std::size_t(m)][0] == 0xff) continue;
        figs_.x[std::size_t(k)] = move_x(at.x, tables_.mech_formation_x[std::size_t(k)]);
        figs_.y[std::size_t(k)] = move_y(at.y, tables_.mech_formation_y[std::size_t(k)]);
        ++figs_.active[std::size_t(k)];
        ++k;
    }

    // 2.3: the opponents (15ab:0d3d), then whether Jason can reach them.
    if (!place_encounter()) return;
    bool any = false;
    MapPosition first{};
    for (int f = 12; f < 24 && !any; ++f)
        if (figs_.active[std::size_t(f)] != 0) {
            any = true;
            first = {figs_.x[std::size_t(f)], figs_.y[std::size_t(f)]};
        }
    if (any) {
        // 203b:28db: from Jason's figure (his 'Mech's when he is in one).
        const int jason = characters_[0][character::kMech] == character::kOnFoot ? 4 : characters_[0][character::kMech];
        const MapPosition from{figs_.x[std::size_t(jason)], figs_.y[std::size_t(jason)]};
        if (from != position_) {
            log_.push_back("an encounter's reach check from away from the party: not rebuilt");
            fight_unfinished_ = true;
            return;
        }
        Combat combat = combat_rules();
        if (combat.reaches(first)) {
            log_.push_back("a random encounter's fight: not rebuilt");
            fight_unfinished_ = true;
            return;
        }
    }
    // 2.12: no fight; the view goes back to the entry position (203b:2aa3).
    position_ = at;
    build_window();
    log_.push_back(any ? "a random encounter: out of reach" : "a random encounter: nobody placed");
    fight_ends();
}

// BTECH.EXE 15ab:0d3d (Ghidra FUN_15ab_0d3d)
// The encounter's opponents: enemy people in character records 8-15 and
// 'Mechs in records 4-7, then placed on free cells of the view, starting
// 10-17 half-cells from the party (docs/combat.md 2.3). False when the
// placing looks past what the rebuild holds.
bool Town::place_encounter() {
    build_window();  // 287f:1314, 1df8 at the party
    int dx = (random_.next() & 7) + 10;
    if (random_.next() & 1) dx = -dx;
    int dy = (random_.next() & 7) + 10;
    if (random_.next() & 1) dy = -dy;
    const int row_start = dx + 0x1a;
    int cx = row_start, cy = dy + 0x0c, count = 0;
    for (int f = 12; f < 24; ++f) {
        figs_.active[std::size_t(f)] = 0;
        figs_.x[std::size_t(f)] = figs_.y[std::size_t(f)] = 0xffff;
    }
    // Soldiers: one in two records.
    for (int c = 8; c < 16; ++c) {
        auto& r = characters_[std::size_t(c)];
        r[character::kName] = character::kEmpty;
        if (!(random_.next() & 1)) continue;
        r[character::kName] = 1;
        r[character::kMech] = character::kOnFoot;
        int sum = 0;
        for (int i = 0; i < 7; ++i) sum += random_.next() & 3;
        r[character::kWeapon] = std::uint8_t(tables_.encounter_weapons[std::size_t(sum)]);
        r[character::kB] = std::uint8_t(two_d6(random_));
        r[character::kHitPoints] = std::uint8_t(10 * r[character::kB]);
        r[character::kD] = std::uint8_t(two_d6(random_));
        for (int i = 0; i < character::kSkillCount; ++i)
            r[std::size_t(character::kSkills + i)] = std::uint8_t(random_.next() & 3);
        r[character::kArmourType] = std::uint8_t(random_.next() & 3);
        const int armour = two_d6(random_);
        r[character::kArmour] = std::uint8_t(armour + two_d6(random_));
    }
    // 'Mechs: one in two, and only as many as the party has (record m - 4).
    for (int m = 4; m < 8; ++m) {
        auto& rec = mechs_[std::size_t(m)];
        rec[0] = 0xff;
        if (!(random_.next() & 1)) continue;
        if (mechs_[std::size_t(m - 4)][0] == 0xff) continue;
        const int pick = random_.next() % 3;  // DG:2df8: templates 0-2
        rec = rules_->mechs[std::size_t(pick)].record;
        figs_.sprite_base[std::size_t(m + 8)] = pick == 0 ? 0 : 0x92;
    }

    const int corner = view_corner_index(position_);
    const int limit = blocking_tile(map_number_);
    int guard = 0;
    auto cell = [&](int index) -> std::optional<int> { return terrain_.window_cell(position_, index); };
    auto next = [&](int by, int wrap) {
        cx += by;
        count += by;
        if (count > wrap) {
            count = 0;
            cx = row_start;
            ++cy;
        }
    };
    auto free = [&](int index) {
        const auto t = cell(index);
        return t && *t > 0x0f && *t < limit && index >= 0;
    };
    // Inside the view's 24 x 24 cells: the column and row the cell is in.
    auto in_view = [&]() {
        int col = ((cx - 0x0d) >> 1) + (corner % kWindowCells);
        if (!(cx & 1) && (position_.x & 1)) ++col;
        int row = (cy >> 1) * kWindowCells + (corner / kWindowCells) * kWindowCells;
        if ((cy & 1) && (position_.y & 1)) row += kWindowCells;
        return col >= 0 && col < kWindowCells && row >= 0 && row < kWindowCells * kWindowCells;
    };
    auto place = [&](int f, int frame) {
        if (in_view()) {
            figs_.x[std::size_t(f)] = view_x(position_.x, cx);
            figs_.y[std::size_t(f)] = view_y(position_.y, cy);
            animation_.set_frame(f, frame);
            figs_.active[std::size_t(f)] = 1;
            return true;
        }
        figs_.x[std::size_t(f)] = figs_.y[std::size_t(f)] = 0xffff;
        figs_.active[std::size_t(f)] = 0;
        return false;
    };
    // The people: the first cell from the cursor whose tile is ground (above
    // 0x0f, below the blocking tile); one dropped outside the view.
    for (int c = 8; c < 16; ++c) {
        if (characters_[std::size_t(c)][character::kName] == character::kEmpty) continue;
        while (!free(window_offset(position_, cx - 0x1a, cy - 0x0c) + corner)) {
            next(1, 16);
            if (++guard > 4096) {
                log_.push_back("an encounter's placing ran past the window: not rebuilt");
                fight_unfinished_ = true;
                return false;
            }
        }
        if (!place(c + 8, 0x10)) characters_[std::size_t(c)][character::kName] = character::kEmpty;
        next(1, 16);
    }
    // The 'Mechs: that cell and the one beside it; 3 on after each.
    cx += 1;
    count += 1;
    for (int m = 4; m < 8; ++m) {
        if (mechs_[std::size_t(m)][0] == 0xff) continue;
        for (;;) {
            const int index = window_offset(position_, cx - 0x1a, cy - 0x0c) + corner;
            const int beside = ((cx ^ position_.x) & 1) == 0 ? index + 1 : index - 1;
            if (free(index) && free(beside)) break;
            next(1, 16);
            if (++guard > 4096) {
                log_.push_back("an encounter's placing ran past the window: not rebuilt");
                fight_unfinished_ = true;
                return false;
            }
        }
        if (!place(m + 8, 0)) mechs_[std::size_t(m)][0] = 0xff;
        next(3, 8);
    }
    return true;
}

}  // namespace chr::game
