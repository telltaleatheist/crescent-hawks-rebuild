// The Star League cache, map 14: going in and its doors (docs/cache.md).

#include <cstdio>

#include "game/town.h"

namespace chr::game {

namespace {

constexpr int kCacheMap = 14;
constexpr std::uint8_t kCacheGrid = 0xd0;  // every grid entry while in the cache
constexpr int kWayInDoor = 11;             // the door that needs no items
constexpr int kDoors = 12;

}  // namespace

// BTECH.EXE 1b5d:0004 (Ghidra FUN_1b5d_0004)
// Into the cache: the 'Mechs parked outside, everyone on foot, map 14 alone
// in the middle grid with the party at its way in, the way-in door opening,
// then INSTRUCT, and the doors already open shown open again.
bool Town::enter_cache(KeySource& keys) {
    for (std::size_t m = 0; m < 4; ++m) {
        parked_mechs_[m] = mechs_[m][0];
        mechs_[m][0] = 0xff;
    }
    for (std::size_t i = 0; i < 8; ++i) characters_[i][character::kMech] = 0;
    position_ = {0x0c06, 0xc07e};
    // 287f:00d1 (CGA colours) and 1000:28cc (the disk) have nothing to do here.
    terrain_.fill_grids(kCacheGrid);
    load_town_map(kCacheMap, 4);
    terrain_.queued_grids.fill(0xff);
    // 273d:063b, 049d and 287f:0260 load the tile set (STARLEAG.ICN, with
    // the map); F:3988 = 2 stops the tile animation (tiles_animate).
    terrain_.link_blocks();  // 287f:1da8
    build_window();          // 287f:1314
    mission_draw();          // 287f:18ef, 273d:06c3
    tick_redraws_ = true;
    hold_screen(60);         // 273d:0006(0x3c)
    if (!open_door(0x0c03, 0x097d, -1, keys)) return false;
    for (std::size_t i = 0; i < 8; ++i) characters_[i][character::kMech] = character::kOnFoot;
    if (!visit_place(0x17, keys)) return false;
    for (int d = 0; d < kDoors - 1; ++d)
        if (flags_[std::size_t(flag::kCacheDoors + d)] != 0 && !open_door(0, 0, d, keys)) return false;
    return true;
}

// BTECH.EXE 1b5d:0ab6 (Ghidra FUN_1b5d_0ab6)
// Door `door`, or with -1 the door at half-cells (x, y) that a step reached:
// unless it is the way in, the three items carried must be the door's own,
// or it says which are wrong and the step ends. An opened door marks its
// items used (the way in empties the slots instead), sets its flag, and its
// four tiles go through three frames into the map (only the last without a
// step, and then without the sound and waits).
bool Town::open_door(int x, int y, int door, KeySource& keys) {
    int found = door;
    std::array<int, 3> needs{};
    if (door < 0) {
        x = (x + 1) & 0x7f;
        y &= 0x7e;
        found = -1;
        for (int d = 0; d < kDoors; ++d)
            if (tables_.door_y[std::size_t(d)] == y && tables_.door_x[std::size_t(d)] == (x & 0x7e)) {
                found = d;
                break;
            }
        if (found < 0) return true;
        for (std::size_t s = 0; s < 3; ++s) needs[s] = tables_.door_items[s][std::size_t(found)] - 1;
        if (found != kWayInDoor) {
            bool wrong = false;
            for (std::size_t s = 0; s < 3; ++s) wrong = wrong || flags_[std::size_t(flag::kCacheItems) + s] != std::uint8_t(needs[s]);
            if (wrong) {
                open_message_box();  // 24d3:17c6
                for (std::size_t s = 0; s < 3; ++s)
                    if (flags_[std::size_t(flag::kCacheItems) + s] != std::uint8_t(needs[s]))
                        print_id("cache.door_wants." + std::to_string(s));  // 2656:03f5
                // 273d:086a, then a key (273d:0259); the step ends (F:d55c).
                turn_message_ = true;
                if (!read_key(keys)) return false;
                return true;
            }
        }
    } else {
        x = tables_.door_x[std::size_t(door)];
        y = tables_.door_y[std::size_t(door)];
        for (std::size_t s = 0; s < 3; ++s) needs[s] = tables_.door_items[s][std::size_t(door)] - 1;
    }
    if (found == kWayInDoor) {
        for (std::size_t s = 0; s < 3; ++s) flags_[std::size_t(flag::kCacheItems) + s] = 0xff;
    } else {
        for (const int item : needs) item_used_[std::size_t(item)] = 1;
    }
    flags_[std::size_t(flag::kCacheDoors + found)] = 1;
    // 1000:19bf(10) from a step: the door's sound, not played here.
    const int column = (x >> 1) & 7;
    const std::size_t corner = std::size_t(((y & 0x70) << 5) + ((y & 0x0e) << 2) + ((x & 0x70) << 2) + column);
    for (int frame = door < 0 ? 0 : 2; frame < 3; ++frame) {
        // Four across; past the block's eighth column, on into the next block.
        std::size_t at = corner;
        for (int i = 0; i < 4; ++i) {
            set_map_cell(at, tables_.door_tiles[std::size_t(frame * 4 + i)]);
            at += column + i + 1 == 8 ? 0x39 : 1;
        }
        build_window();  // 287f:1314
        mission_draw();  // 287f:18ef, 1000:051b, 273d:06c3
        tick_redraws_ = true;
        if (door < 0) hold_screen(20);  // 273d:0006(0x14)
    }
    return true;
}

// BTECH.EXE 1b5d:03aa (Ghidra FUN_1b5d_03aa)
// The items whose three half-cells hold (x, y): each in turn, unless a door
// has used it, offers itself for its slot, and yes puts it there in place
// of what the slot held. A used item says so and waits for a key.
bool Town::take_item(int x, int y, KeySource& keys) {
    x = (x + 1) & 0x7f;
    y &= 0x7f;
    for (std::size_t i = 0; i < item_used_.size(); ++i) {
        const int left = tables_.item_x[i];
        if (tables_.item_y[i] != y || x < left || x >= left + 3) continue;
        open_message_box();  // 24d3:17c6
        if (item_used_[i] == 0) {
            const int slot = tables_.item_slot[i];
            print_id("cache.item_take");                             // 2656:03f5
            print_id("cache.item_kind." + std::to_string(slot));     // through DG:2234
            print_id("cache.item_number");
            text_.print(std::to_string(i + 1));                      // 273d:0053
            print_id("cache.item_take_end");
            const auto yes = yes_no(true, keys);                     // 1000:1a13(1)
            if (!yes) {
                place_open_ = true;
                return false;
            }
            if (*yes) flags_[std::size_t(flag::kCacheItems + slot)] = std::uint8_t(i);
        } else {
            print_id("cache.item_used");  // 24d3:17ea
            if (!read_key(keys)) return false;  // 273d:0259
        }
        turn_message_ = true;  // F:d55c
    }
    return true;
}

// BTECH.EXE 1b5d:01e9 (Ghidra FUN_1b5d_01e9)
// The wall at cells x 0-2, y 42-47 becomes the six rows of three tiles in
// the table, the window is redrawn, and the text says so.
bool Town::open_cache_wall(KeySource& keys) {
    for (std::size_t row = 0; row < 6; ++row)
        for (std::size_t col = 0; col < 3; ++col)
            set_map_cell(0xa10 + row * 8 + col, tables_.cache_wall[row * 3 + col]);
    build_window();  // 287f:1314
    mission_draw();  // 287f:18ef, 1000:051b, 273d:06c3
    tick_redraws_ = true;
    return cache_message("cache.wall_opened", keys);
}

// BTECH.EXE 1b5d:02d2 (Ghidra FUN_1b5d_02d2)
// Only at its own place (x 4e, y 0c-10 in half-cells): the text, and from
// then on the way out answers.
bool Town::cache_hint(int x, int y, KeySource& keys) {
    x = (x + 1) & 0x7e;
    y &= 0x7e;
    if (x != 0x4e || y < 0x0c || y > 0x11) return true;
    if (!cache_message("cache.hint", keys)) return false;
    flags_[flag::kCacheHint] = 1;
    return true;
}

// The message box, the text (24d3:17ea) and a key (273d:0259); the step
// shows no direction (F:d55c).
bool Town::cache_message(const std::string& id, KeySource& keys) {
    open_message_box();  // 24d3:17c6
    print_id(id);
    turn_message_ = true;
    return read_key(keys).has_value();
}

// BTECH.EXE 1000:1c12 (Ghidra FUN_1000_1c12), while F:d346 is set
// The tile `tile` that stopped a character's step, at half-cell `at`, does
// what it does on the cache's level; any other tile only blocks.
bool Town::cache_step(int tile, formats::MapPosition at, KeySource& keys) {
    const int x = std::int16_t(at.x), y = std::int16_t(at.y);
    const auto not_yet = [&](const char* what) {
        char line[64];
        std::snprintf(line, sizeof line, "cache tile %02x at %04x,%04x: %s", tile, at.x, at.y, what);
        log_.push_back(line);
        return true;
    };
    if (flags_[flag::kCacheLevel] == 0) {
        // A door's three tiles, each naming the door from its own cell.
        if (tile >= 0x7e && tile <= 0x80) return open_door(x - 2 * (tile - 0x7e), y, -1, keys);
        if (tile == 0xf5 && at.x >= 0x0c01 && at.x <= 0x0c04 && at.y == 0xc054) return open_cache_wall(keys);
        if (tile == 0xb6 || tile == 0xb7) return take_item(x, y, keys);
        if (tile >= 0xf6) return not_yet("1b5d:0288");
        if (tile == 0x83 || (tile >= 0xa5 && tile <= 0xa7)) return cache_message("cache.screen", keys);  // 1b5d:02a8
        if (tile == 0x4d) return not_yet("1b5d:04ab");
        if (tile == 0x3a || tile == 0x3d) return cache_hint(x, y, keys);
        if (tile == 0x28) return not_yet("1b5d:055a");
        return true;
    }
    if (tile == 0x94) return not_yet("1b5d:079c");
    if (tile >= 0x97 && tile <= 0xf0) return not_yet("1b5d:0913");
    if (tile == 0x8c || tile == 0x8d) return not_yet("1b5d:0980");
    return true;
}

}  // namespace chr::game
