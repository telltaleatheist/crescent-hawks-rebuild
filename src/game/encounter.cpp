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
// encounter outdoors (docs/combat.md 2.1-2.3, 2.12, 2.13). False when the
// keys ran out or the game stopped.
bool Town::random_encounter(KeySource& keys) {
    const MapPosition at = position_;
    encounter_entry_ = at;
    // 2.1, 3: the window's nine block values (P:07a4) as they are.
    const auto& blocks = terrain_.held_window(window_at_).blocks;
    for (std::size_t i = 0; i < encounter_blocks_.size(); ++i) encounter_blocks_[i] = std::uint8_t(blocks[i]);
    // The encounter comes before the tick draws the map (1000:046e): the
    // screen still shows the map as the last tick drew it.
    // The page (the drawing buffer) holds that drawing too.
    if (last_drawn_ && !frozen_) {
        encounter_frozen_ = true;
        frozen_ = last_drawn_;
        page_ = last_drawn_;
        page_sprites_.clear();
    }
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

    // 2.3: the opponents (15ab:0d3d); the box above the party box is
    // cleared for the attackers (2656:0281(4), 0388).
    if (!place_encounter()) return false;
    text_.select_window(4);
    text_.clear_window();
    bool any = false;
    MapPosition first{};
    for (int f = 12; f < 24 && !any; ++f)
        if (figs_.active[std::size_t(f)] != 0 && figs_.x[std::size_t(f)] != 0xffff && figs_.y[std::size_t(f)] != 0xffff) {
            any = true;
            first = {figs_.x[std::size_t(f)], figs_.y[std::size_t(f)]};
        }
    bool reached = false;
    if (any) {
        // 203b:28db: from Jason's figure (his 'Mech's when he is in one).
        const int jason = characters_[0][character::kMech] == character::kOnFoot ? 4 : characters_[0][character::kMech];
        // The position walks there and the window is built there (1000:17bb,
        // 287f:1314, 1df8); afterwards it walks back and the window is built
        // again (203b:2e53).
        walk_position({figs_.x[std::size_t(jason)], figs_.y[std::size_t(jason)]});
        build_window();
        Combat combat = combat_rules();
        reached = combat.reaches(first);
        walk_position(at);
        build_window();
    }
    if (!reached) {
        // 2.12: no fight; the view goes back to the entry position (203b:2aa3).
        walk_position(at);  // 203b:2e53, 1000:17bb
        build_window();
        log_.push_back(any ? "a random encounter: out of reach" : "a random encounter: nobody placed");
        fight_ends();
        return true;
    }
    question_waiting_ = true;
    // 2.3, 4-6: the attackers, in the box above the party box.
    print_id("fight.attackers");
    int mechs = 0;
    for (int m = 4; m < 8; ++m)
        if (mechs_[std::size_t(m)][0] != 0xff) ++mechs;
    if (mechs > 0) {
        text_.print(std::to_string(mechs));  // 273d:0053
        print_id("fight.attackers_mech");
        if (mechs > 1) print_id("ui.plural_s");  // 1000:2a69
        print_id("fight.attackers_and");
    }
    int people = 0;
    for (int c = 8; c < 16; ++c) {
        const int f = c + 8;
        if (characters_[std::size_t(c)][character::kName] != character::kEmpty && figs_.active[std::size_t(f)] != 0 &&
            figs_.x[std::size_t(f)] != 0xffff && figs_.y[std::size_t(f)] != 0xffff)
            ++people;
    }
    text_.print(std::to_string(people));
    print_id("fight.attackers_human");
    if (people != 1) print_id("ui.plural_s");
    print_id("ui.full_stop");  // 1000:2a7e
    log_.push_back("a random encounter in reach: " + std::to_string(mechs) + " 'Mechs, " + std::to_string(people) +
                   " people");
    text_.select_window(3);
    text_.clear_window();
    party_box_shown_ = false;
    print_id("fight.engage");
    const auto engage = yes_no(true, keys);  // 1000:1a13(1)
    if (!engage) {
        place_open_ = true;
        return false;
    }
    if (!*engage) {
        // Declined: three in four get away.
        if ((random_.next() & 3) != 0) return encounter_end(false, keys);
        clear_window();
        print_id("fight.not_evaded");
        if (!press_key(keys)) return false;
    }
    return fight(0, keys);
}

// BTECH.EXE 203b:000a, its end for kinds 0 and 3 (203b:13e7..17ca,
// docs/combat.md 2.12): after a fight, the after-fight steps (2.15); after
// eluding, the message. Then the return to the entry position and every
// fight's end (2.13).
bool Town::encounter_end(bool fought, KeySource& keys) {
    text_.select_window(3);
    clear_window();
    if (fought && combat_.fight_on) {
        text_.select_window(1);
        text_.frame(1);  // 2656:0004(1)
        clear_window();
        bool beaten = false;
        for (int f = 12; f < 24; ++f) beaten = beaten || figs_.destroyed[std::size_t(f)];
        if (beaten) {
            print_id("fight.beat_people");
            if (!press_key(keys)) return false;
        }
        if (!after_fight_steps(keys)) return false;
    } else if (combat_.fight_on) {
        clear_window();
        print_id("fight.eluded");
        if (!press_key(keys)) return false;
    }
    if (combat_.fight_on) {
        text_.select_window(4);
        text_.clear_window();
        draw_panel(true);  // 1000:4cac(1)
        open_message_box();
        print_id("fight.return");
        if (!read_key(keys)) {
            place_open_ = true;
            return false;
        }
        walk_position(encounter_entry_);  // 1000:17bb
        reload_areas();                // 203b:2835
        build_window();
    }
    fight_ends();
    return true;
}

// BTECH.EXE 203b:2835 (Ghidra FUN_203b_2835)
// The maps of the nine areas around the party loaded again, each into its
// grid unless the grid's first entry is already 0x90, then the masks.
void Town::reload_areas() {
    const int area = ((position_.y >> 8) & 0xf0) | ((position_.x >> 8) & 0x0f);
    for (int k = 0; k < 9; ++k) {
        const int a = area - 0x11 + 16 * (k / 3) + k % 3;
        if (a < 0 || a >= 0x100 || tables_.area_maps[std::size_t(a)] == 0) continue;
        if (terrain_.grids()[std::size_t(k * Terrain::kGridBlocks)] == 0x90) continue;
        load_town_map(tables_.area_maps[std::size_t(a)], k);
    }
    terrain_.link_blocks();
}

// BTECH.EXE 203b:1431..166d, kinds 0 and 3 after a fight won (docs/combat.md
// 2.15): repairs and scrap, salvage, loot, flag 29, healing.
bool Town::after_fight_steps(KeySource& keys) {
    bool has_mech = false, wreck = false;
    for (int m = 0; m < 4; ++m) has_mech = has_mech || mechs_[std::size_t(m)][0] != 0xff;
    for (int f = 12; f < 16; ++f) wreck = wreck || figs_.destroyed[std::size_t(f)];
    if (has_mech && wreck)
        for (int c = 0; c < 8; ++c)
            if (characters_[std::size_t(c)][0] != 0xff && characters_[std::size_t(c)][character::kSkills + 5] != 0) {
                log_.push_back("repairs and scrap (15ab:0002): not rebuilt");
                fight_unfinished_ = true;
                return false;
            }
    bool mech_down = false;
    for (int m = 0; m < 4; ++m) mech_down = mech_down || figs_.destroyed[std::size_t(m)] || figs_.destroyed[std::size_t(12 + m)];
    if (mech_down) {
        bool pilot = false, free_slot = false;
        for (int c = 0; c < 8; ++c) {
            const auto& r = characters_[std::size_t(c)];
            if (r[0] != 0xff && r[character::kSkills + 4] != 0 && r[character::kMech] >= character::kOnFoot) pilot = true;
        }
        for (int m = 0; m < 4; ++m) free_slot = free_slot || mechs_[std::size_t(m)][0] == 0xff;
        // Not among the town's blocks (a map block's value has bit 0x80).
        for (const auto b : encounter_blocks_) free_slot = free_slot && (b & 0x80) == 0;
        if (pilot && free_slot) {
            log_.push_back("salvage (15ab:04f9): not rebuilt");
            fight_unfinished_ = true;
            return false;
        }
    }
    if (!loot(keys)) return false;
    flags_[0x29] = 0;
    text_.select_window(3);
    clear_window();
    int hurt = 0;
    for (int c = 0; c < 8; ++c) {
        const auto& r = characters_[std::size_t(c)];
        if (r[0] == 0xff) continue;
        hurt += std::int8_t(std::uint8_t(10 * r[character::kB])) - std::int8_t(r[character::kHitPoints]);
    }
    if (hurt != 0) return heal(0, keys);  // 1c31:000a(0)
    return true;
}

// BTECH.EXE 1c31:000a (Ghidra FUN_1c31_000a)
// Healing (docs/combat.md 2.15): the party's best medic (byte 0x0a), or a
// doctor of level `doctor`, treats everyone hurt with dice by skill and
// care. With flag 29 set it only says whether anyone is still hurt.
bool Town::heal(int doctor, KeySource& keys) {
    const auto name = [this](int c) {
        print_id("character." + std::to_string(characters_[std::size_t(c)][character::kName]));
    };
    if (flags_[0x29] != 0) {
        open_message_box();  // 24d3:17c6 / 17ea, 273d:0259
        print_id(anyone_hurt() ? "heal.more_time" : "heal.nobody_2");
        if (!read_key(keys)) {
            place_open_ = true;
            return false;
        }
        return true;
    }
    int skill = 0, medic = 0;
    for (int c = 0; c < 8; ++c) {
        const auto& r = characters_[std::size_t(c)];
        if (r[character::kName] == character::kEmpty) continue;
        const int s = std::int8_t(r[character::kSkills + 6]);
        if (skill < s) {
            medic = c;
            skill = s;
        }
    }
    flags_[0x19] = anyone_hurt() ? 1 : 0;  // special 23
    if (flags_[0x19] == 0) {
        if (doctor != 0) return true;
        open_message_box();
        print_id("heal.nobody");
        if (!read_key(keys)) {  // 1000:2a2b's flush, then 273d:0259
            place_open_ = true;
            return false;
        }
        return true;
    }
    text_.select_window(1);
    text_.frame(1);
    clear_window();
    // The care: torn cloth, a MedKit (F:d450), a field surgery kit (F:d451,
    // skill 3 or more), or a doctor's. Nothing rebuilt sets d450 or d451
    // yet (docs/scripts.md, "Not rebuilt yet").
    const bool medkit = false, surgery_kit = false;
    int care = 1;
    if (medkit) care = 2;
    if (skill >= 3 && surgery_kit) care = 3;
    if (doctor != 0) {
        care = doctor > 2 ? 4 : 2;
        print_id("heal.doctor");
        skill = doctor;
    } else {
        name(medic);
    }
    if (skill != 0) {
        print_id("heal.uses");
        print_id(skill < 3 ? "heal.skill." + std::to_string(skill) : skill < 5 ? "heal.skill." + std::to_string(skill - 1)
                                                                               : std::string("heal.skill.5"));
        print_id("heal.skill_and");
        print_id("heal.care." + std::to_string(care));
    } else {
        print_id("heal.no_skill");
    }
    print_id("heal.to_assist");
    text_.set_fg(10);  // 1000:28a2
    for (int c = 0; c < 8; ++c) {
        auto& r = characters_[std::size_t(c)];
        if (r[character::kName] == character::kEmpty) continue;
        const int most = std::int8_t(r[character::kB]) * 10;
        if (std::int8_t(r[character::kHitPoints]) == most) continue;
        print_id("talk.line_end");  // 24d3:181e
        name(c);
        print_id("heal.healing");
        // DG:2601 + 4 x skill + care: the low nibble d6s, times the high
        // nibble when it is not 0 (sign-extended first, as the original).
        const int at = 4 * skill + care - 1;
        if (at < 0 || at >= int(tables_.heal_dice.size())) {
            log_.push_back("healing past the dice table: not rebuilt");
            fight_unfinished_ = true;
            return false;
        }
        int dice = std::int8_t(std::uint8_t(tables_.heal_dice[std::size_t(at)]));
        int times = 1;
        if ((dice & 0xf0) != 0) {
            times = (dice >> 4) & 0x0f;
            dice &= 0x0f;
        }
        int gain = 0;
        for (int n = 0; n < dice; ++n) gain += d6(random_);  // 1000:19f3
        int hp = std::int8_t(r[character::kHitPoints]) + gain * times;
        if (hp > most) hp = most;
        r[character::kHitPoints] = std::uint8_t(hp);
    }
    text_.set_fg(15);
    if (!press_key(keys)) return false;  // 273d:086a, 1000:2a4f
    draw_panel(false);  // 1000:4cac(0)
    flags_[0x29] = 0x3f;
    return true;
}

// BTECH.EXE 15ab:094b (Ghidra FUN_15ab_094b)
bool Town::loot(KeySource& keys) {
    // Character record r's byte b as the original reads it: records past
    // the sixteen run on into the 'Mech records (F:c724).
    const auto record_byte = [this](int r, int b) -> int {
        if (r < 16) return characters_[std::size_t(r)][std::size_t(b)];
        const int at = (r - 16) * 0x11 + b;
        return mechs_[std::size_t(at / 0x7d)][std::size_t(at % 0x7d)];
    };
    bool anyone = false, weapons = false;
    int beaten = 0;
    for (int s = 4; s < 12; ++s) {
        if (figs_.destroyed[std::size_t(s)] || figs_.destroyed[std::size_t(s + 12)]) anyone = true;
        if (!figs_.destroyed[std::size_t(s + 12)]) continue;
        // Bug 27: character s + 12, not s + 4 (enemy person s + 12 is
        // character s + 4).
        if (record_byte(fixes_.fix_loot_index ? s + 4 : s + 12, character::kWeapon) != 0) weapons = true;
        ++beaten;
    }
    if (!anyone) return true;
    text_.select_window(1);
    clear_window();
    text_.frame(0);
    print_id("after.loot.pockets");
    int cash = 0;
    for (int n = 0; n < beaten; ++n) cash += (random_.next() & 0x0f) + 3;
    cash = std::max(cash, 2);
    text_.set_fg(10);  // 1000:28a2
    text_.print(std::to_string(cash));  // 273d:0053
    text_.set_fg(15);
    print_id("after.loot.cash");
    money_.cbills += cash;  // F:d370, not redrawn here
    if (weapons) print_id("after.loot.weapons");
    print_id("ui.full_stop");  // 1000:2a7e
    if (!read_key(keys)) {  // 273d:086a, 273d:0259
        place_open_ = true;
        return false;
    }
    if (!weapons) return true;
    for (int p = 0; p < 8; ++p) {
        if (!figs_.destroyed[std::size_t(16 + p)]) continue;
        const int found = std::int8_t(characters_[std::size_t(8 + p)][character::kWeapon]);
        if (characters_[1][0] != 0xff) {
            log_.push_back("a found weapon offered to the party (17dc:15e6): not rebuilt");
            fight_unfinished_ = true;
            return false;
        }
        // Jason alone: a swap for his own weapon.
        clear_window();
        if (found == 0) continue;
        const int own = std::int8_t(characters_[0][character::kWeapon]);
        print_id("after.loot.swap_question");
        print_id(formats::weapon_text_id(own));
        print_id("after.loot.swap_for");
        print_id(formats::weapon_text_id(found));
        print_id("after.loot.swap_end");
        const auto swap = yes_no(false, keys);  // 1000:1a13(0)
        if (!swap) {
            place_open_ = true;
            return false;
        }
        if (*swap) characters_[0][character::kWeapon] = std::uint8_t(found);
    }
    return true;
}

// BTECH.EXE 15ab:0d3d (Ghidra FUN_15ab_0d3d)
// The encounter's opponents: enemy people in character records 8-15 and
// 'Mechs in records 4-7, then placed on free cells of the view, starting
// 10-17 half-cells from the party (docs/combat.md 2.3). False when the
// placing looks past what the rebuild holds.
bool Town::place_encounter() {
    build_window();  // 287f:1314, 1df8 at the party
    terrain_.corner_words_set = true;
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
