// The training missions' driver (docs/combat.md 1.3): a mission's set-up,
// the walk it shares with every mode, and the results.
#include "formats/game_files.h"
#include "formats/mtp.h"
#include "game/text_provider.h"
#include "game/tile_animation.h"
#include "game/town.h"

#include <cstdio>

namespace chr::game {

namespace {

// P:200a, the map store's byte 0xfed (block 63, row 5, column 5): the
// course clears it while it runs and puts it back after.
constexpr std::size_t kCourseCell = 0xfed;

// The door of the training center: a walk back to it ends a mission.
bool at_the_door(formats::MapPosition p) { return p.x <= 0x0c3c && p.y >= 0xc049 && p.y <= 0xc04f; }

}  // namespace

// The map store changes at once; the cell window (P:07ad) only when
// 1000:218f next builds it, unless `rebuild`.
void Town::set_map_cell(std::size_t index, int tile, bool rebuild) {
    map_.blocks[index] = std::uint8_t(tile);
    if (rebuild) {
        terrain_.map_changed();
    } else {
        window_stale_ = true;
    }
}

// BTECH.EXE 17dc:0629 (Ghidra FUN_17dc_0629)
// Sets up the mission of `mode`, then walks: a key is a step, and without
// one the tiles animate and the townspeople move. A mission ends when the
// party is back at the training center's door once it is done; then the
// results.
bool Town::run_mission(int mode, KeySource& keys) {
    MissionWalk w;
    w.mode = mode;
    switch (mode) {
    case 0:
        if (map_.blocks.size() > kCourseCell) {
            w.saved_cell = map_.blocks[kCourseCell];
            set_map_cell(kCourseCell, 0);
        }
        w.cell = kCourseCell;
        // 1000:218f(0): a step on the spot.
        if (!step_party({0, 0}, keys)) return false;
        break;
    case 1: {
        // One of eight spots is marked with tile 0x50.
        w.spot = random_.next() & 7;
        const int sx = tables_.marked_spot_x[std::size_t(w.spot)];
        const int sy = tables_.marked_spot_y[std::size_t(w.spot)];
        w.cell = std::size_t(((sy & 0x78) << 6) + ((sy & 7) << 3) + ((sx & 0x78) << 3) + (sx & 7));
        log_.push_back("mission 1: spot " + std::to_string(w.spot) + " (" + std::to_string(sx) + ", " +
                       std::to_string(sy) + ")");
        if (map_.blocks.size() > w.cell) {
            w.saved_cell = map_.blocks[w.cell];
            set_map_cell(w.cell, 0x50);
        }
        if (mechs_[0][0] == 'L') {
            open_message_box();
            print_id("mission.failed");  // 24d3:17ea, then a key
            if (!read_key(keys)) return false;
        }
        if (!step_party({0, 0}, keys)) return false;
        break;
    }
    case 2: {
        place_mission(1, 0);
        // The target can't shoot back: 'Mech record 4 loses its weapons and
        // ammunition (bytes 0x24, 0x25 and 0x27-0x32; 0x26 stays).
        auto& target = mechs_[4];
        target[0x24] = target[0x25] = 0;
        for (int b = 0x27; b <= 0x32; ++b) target[std::size_t(b)] = 0;
        if (!fight(1, keys)) return false;
        party_to_jason();
        w.done = true;
        break;
    }
    case 3:
        place_mission(0, (random_.next() & 3) + 3);
        if (!fight(1, keys)) return false;
        party_to_jason();
        w.done = true;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        // 1-4 enemy 'Mechs. Mission 4 alone (17dc:0765; 5-7 enter at 0770)
        // has no computer to fight for the party (F:0090). Mission 6 is the
        // raid one time in two, mission 7 always.
        no_computer_order_ = mode == 4;
        if (mode == 6 && (random_.next() & 1)) raid_ = true;
        if (mode == 7) raid_ = true;
        // The raid: the map store's last 16 cells (block 63's last two
        // rows) become tile 0x40.
        if (raid_)
            for (std::size_t c = 0xff0; c < 0x1000 && c < map_.blocks.size(); ++c) set_map_cell(c, 0x40);
        place_mission(mode - 3, 0);
        if (!fight(1, keys)) return false;
        party_to_jason();
        w.done = true;
        no_computer_order_ = false;
        break;
    default:
        log_.push_back("mission " + std::to_string(mode) + ": not rebuilt");
        fight_unfinished_ = true;
        return false;
    }
    if (!mission_walk(w, keys)) return false;
    mission_results(w);
    return true;
}

bool Town::mission_walk(MissionWalk& w, KeySource& keys) {
    // The raid (F:3772) and the Arena go straight to the results.
    bool end = raid_ || w.mode == 8;
    int countdown = 0;  // [bp-1e]
    while (!end) {
        if (at_the_door(position_)) {
            if (w.done) {
                end = true;
                continue;
            }
            if (!w.warned) {
                // 24d3:17c6, then 2656:03f5 (no pause of its own), 273d:086a
                // and a key.
                open_message_box();
                print_id("mission.not_done");
                if (!read_key(keys)) return false;
            }
            w.warned = true;
        }
        // 273d:002f: a key waiting?
        const auto waiting = keys.waiting_key();
        if (!waiting) {
            place_open_ = true;
            return false;
        }
        if (*waiting != KeySource::kNoKey) {
            // 2656:0d1d, 1000:2a2b (the rest of the keyboard is thrown away),
            // 1000:231d, 1000:218f.
            const int k = arrow_key(*waiting);
            animation_.key(k, mech_records());
            const auto step = step_for_key(k);
            if (!step_party(step.value_or(Step{}), keys)) return false;
        } else if (--countdown >= 0) {
            continue;  // 273d:0006(1): a frame passes
        }
        countdown = 10;
        mission_tick();
        if (!mission_step_done(w, keys)) return false;
    }
    return true;
}

// The walk's tick: the tiles' next frame (1000:240b), every fourth tick the
// townspeople move (1000:24c2, counted in DG:5802), and the map and the
// figures are drawn (287f:18ef, 1000:051b, 273d:06c3).
void Town::mission_tick() {
    if (tiles_animate(map_number_)) phase_ = (phase_ + 1) % 3;
    if (++mission_ticks_ == 1) move_townspeople();
    mission_ticks_ &= 3;
    mission_draw();
}

// 287f:18ef, 1000:051b, 273d:06c3: the map and the figures drawn.
void Town::mission_draw() {
    const auto figures = walk_in_figures();
    record_party_places();
    text_.forget(13, 0, kTextColumns - 13, kTextRows);
    frozen_ = Frozen{live_view(), figures};
}

// BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8) into grid 4, and 287f:1da8
// Map `number` takes the town's place: its blocks in the middle grid, its
// tiles (map 11 has its own sheet), its blocking tile and its townspeople,
// placed again with eight random numbers (none out on maps above 10).
bool Town::load_town_map(int number) {
    const std::string name = "MAP" + std::to_string(number) + ".MTP";
    const auto path = game_dir_ ? formats::find_game_file(*game_dir_, name) : std::nullopt;
    if (!path) {
        log_.push_back(name + ": not found");
        return false;
    }
    map_ = formats::decode_mtp(formats::read_file(*path));
    map_number_ = number;
    tile_sheet_ = tile_sheet_for_map(number);
    flags_[flag::kTileEvents] = number == 14 ? 1 : 0;
    terrain_.hold_window();
    terrain_.place_map(map_, 4);
    terrain_.link_blocks();
    if (townspeople_away_) {
        people_.place_during_fight(map_, number, random_);
        for (int i = 0; i < 8; ++i) {
            const auto& w = people_.walkers()[std::size_t(i)];
            const auto at = number > 10 ? formats::MapPosition{0, 0} : w.target;
            figs_.x[std::size_t(16 + i)] = at.x;
            figs_.y[std::size_t(16 + i)] = at.y;
            animation_.set_direction(16 + i, -1);
            animation_.set_frame(16 + i, 0x10);
        }
    } else {
        log_.push_back("a map load in town: not rebuilt");
    }
    log_.push_back("map " + std::to_string(number) + " loaded");
    return true;
}

// BTECH.EXE 17dc:134b (Ghidra FUN_17dc_134b)
// After a fight the party stands where Jason's figure is: his 'Mech's
// (figure 0, two half-cells down, into the next area if that crosses its
// edge) or his own (figure 4) when he is on foot. The window is built
// there and the map drawn.
void Town::party_to_jason() {
    if (characters_[0][character::kMech] != character::kOnFoot) {
        std::uint16_t y = std::uint16_t(figs_.y[0] + 2);
        if (y & 0x80) y = std::uint16_t(y + 0x0f80);
        position_ = {figs_.x[0], y};
    } else {
        position_ = {figs_.x[4], figs_.y[4]};
    }
    window_at_ = position_;  // 287f:1314
    if (window_stale_) {
        terrain_.map_changed();
        window_stale_ = false;
    }
    char where[32];
    std::snprintf(where, sizeof where, "back at %04x,%04x", position_.x, position_.y);
    log_.push_back(where);
    mission_draw();
}

// What each mode checks after a tick.
bool Town::mission_step_done(MissionWalk& w, KeySource& keys) {
    switch (w.mode) {
    case 0:
        // The course counts ticks, two more for each in water (F:32ae of
        // figure 0, 8 or 16 by 1000:051b, divided by 8).
        ++w.count;
        if (combat_.terrain[0] != 0) w.count += combat_.terrain[0] >> 3;
        if (position_.x >= 0x0c78 && position_.y >= 0xc07c && !w.done) {
            w.done = true;
            open_message_box();
            print_id("mission.course_corner");  // 24d3:17ea: a pause, then a key
            if (!read_key(keys)) return false;
        }
        return true;
    case 1: {
        // Standing on the cell just below the spot, or below and to the
        // left: in cells of the area, (x & 0x7f) / 2 and (y & 0x7f) / 2.
        const int hx = (position_.x & 0x7f) >> 1, hy = (position_.y & 0x7f) >> 1;
        const int sx = tables_.marked_spot_x[std::size_t(w.spot)];
        const int sy = tables_.marked_spot_y[std::size_t(w.spot)];
        if (hx < sx - 1 || hx > sx || hy != sy + 1 || w.done) return true;
        w.done = true;
        open_message_box();
        if (mechs_[0][0] == 'L') {
            print_id("mission.no_hands");
        } else {
            // The 'Mech picks it up: the spot is put back at once.
            print_id("mission.hands");
            if (w.saved_cell) set_map_cell(w.cell, *w.saved_cell, false);
        }
        return read_key(keys).has_value();  // 273d:086a, 273d:0259
    }
    default:
        return true;
    }
}

// 17dc:0bf2: flag 01 says whether the mission was passed.
void Town::mission_results(const MissionWalk& w) {
    flags_[flag::kPassed] = 0;
    switch (w.mode) {
    case 0: {
        // Below 215, or 265 in a Locust (record 0's name starts with L).
        int count = w.count;
        if (mechs_[0][0] == 'L') count -= 50;
        if (w.done && count < 215) flags_[flag::kPassed] = 1;
        if (w.saved_cell) set_map_cell(w.cell, *w.saved_cell, false);
        break;
    }
    case 1: {
        // Passed unless in a Locust; Piloting goes up to 3 either way.
        if (w.saved_cell) set_map_cell(w.cell, *w.saved_cell, false);
        if (mechs_[0][0] != 'L') flags_[flag::kPassed] = 1;
        auto& piloting = characters_[0][std::size_t(character::kSkills + 4)];
        if (piloting < 3) ++piloting;
        break;
    }
    default:
        if (w.mode > 7) break;
        // Modes 2-7: passed when Jason's 'Mech is still there and nobody
        // fled (F:3992).
        if (mechs_[0][0] != 0xff && !combat_.person_ran) flags_[flag::kPassed] = 1;
        if (w.mode > 4) {
            // Gunnery, then Piloting: + r & 1 each, the draw only below 4.
            for (int s : {3, 4}) {
                auto& skill = characters_[0][std::size_t(character::kSkills + s)];
                if (skill < 4) skill = std::uint8_t(skill + (random_.next() & 1));
            }
        }
        if (raid_) {
            // Outdoors now (flag 04); flag 05 if the 'Mech came through.
            flags_[flag::kOutdoors] = 1;
            if (mechs_[0][0] != 0xff) flags_[flag::kRaidMech] = 1;
        }
        raid_ = false;
        break;
    }
    // The town's own drawing takes over again.
    frozen_.reset();
    // F:398e = 0. F:4594 has the place's script deciphered again
    // (17dc:1d30); the rebuild keeps the script apart from the fight.
    log_.push_back("mission " + std::to_string(w.mode) + ": " + (flags_[flag::kPassed] ? "passed" : "not passed") +
                   " (count " + std::to_string(w.count) + ")");
}

}  // namespace chr::game
