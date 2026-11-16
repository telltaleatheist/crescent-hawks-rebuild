// A training mission from the 'Mech choice to the orders menu (docs/combat.md,
// "Training missions"): special 1 walks the chosen 'Mech in, the mission
// driver places the enemy, and the fight asks its three questions and shows
// the orders menu for the first unit.
#include <algorithm>

#include "game/fight.h"
#include "game/map_view.h"
#include "game/text_provider.h"
#include "game/tile_animation.h"
#include "game/town.h"
#include "game/world.h"

namespace chr::game {

namespace {

// 'Mech record bytes (docs/formats/mech.md).
constexpr int kArmourFirst = 0x11, kArmourLast = 0x1b;
constexpr int kSlots = 0x34;
constexpr int kPilot = 0x79;

std::int16_t wrap16(int v) { return std::int16_t(std::uint16_t(v)); }

// 1000:2689 / 0e4b: a figure's place relative to the view's top-left text
// cell, across area edges as the original works it out.
int view_col(std::uint16_t fx, std::uint16_t px) {
    int col = wrap16(fx - px);
    if (col < -0x80) col += 0x80;
    if (col > 0x80) col -= 0x80;
    return col + 0x1a;
}
int view_row(std::uint16_t fy, std::uint16_t py) {
    int row = wrap16(fy - py);
    if (row < wrap16(0xf080)) row += 0xf80;
    if (row > 0xf80) row -= 0xf80;
    return row + 0x0c;
}

}  // namespace

int mech_sink(int tile, int quarters) {
    if (tile < 0 || tile >= 0xf6 || (tile & quarters) == 0 || (tile & 0xf0) >= 0x30) return 0;
    if (((tile & 0xf0) == 0x00 || (tile & 0xf0) == 0x20) && (tile & 0x0f) == 0x0f) return 16;
    return 8;
}

bool Town::on_foot(int character) const {
    const auto& c = characters_[std::size_t(character)];
    return c[character::kName] != character::kEmpty && c[character::kMech] >= character::kOnFoot;
}

// BTECH.EXE 1000:051b (Ghidra FUN_1000_051b), as drawn during special 1's
// walk-in: the party's characters on foot, the townspeople, then each
// 'Mech of records 0-3 at the k-th 'Mech place around the party (k counts
// the 'Mechs drawn), 24x24 with its foot cell two half-cells below its
// top. Afterwards the 'Mechs' places are recorded as figures 0-3.
std::vector<Figure> Town::walk_in_figures() {
    Party walking = party_;
    for (int i = 0; i < 8; ++i) walking.present[std::size_t(i)] = walking.present[std::size_t(i)] && on_foot(i);
    auto all = party_figures(map_, position_, walking, animation_, tables_);
    const auto people = people_.figures(map_, position_, animation_);
    all.insert(all.end(), people.begin(), people.end());
    const bool odd_x = position_.x & 1, odd_y = position_.y & 1;
    int k = 0;
    for (int m = 0; m < 4; ++m) {
        if (mechs_[std::size_t(m)][0] == 0xff) continue;
        Figure f;
        f.sprite = sprite(figs_.sprite_base[std::size_t(m)] + animation_.frame(m));
        f.half_x = position_.half_cell_x() + tables_.mech_formation_x[std::size_t(k)];
        f.half_y = position_.half_cell_y() + tables_.mech_formation_y[std::size_t(k)] - 2;
        const int index = view_corner_index(position_) + tables_.mech_cell[std::size_t(k)] +
                          (odd_x ? tables_.mech_cell_odd_x[std::size_t(k)] : 0) + (odd_y ? tables_.mech_cell_odd_y[std::size_t(k)] : 0);
        const int quarters = tables_.mech_water[std::size_t(k)] ^ (odd_y ? 5 : 0);
        f.sink = mech_sink(window_cell(map_, position_, index).value_or(-1), quarters);
        all.push_back(f);
        figs_.x[std::size_t(k)] = move_x(position_.x, tables_.mech_formation_x[std::size_t(k)]);
        figs_.y[std::size_t(k)] = move_y(position_.y, tables_.mech_formation_y[std::size_t(k)]);
        figs_.active[std::size_t(k)] = 1;
        ++k;
    }
    return all;
}

// BTECH.EXE 1000:0e4b (Ghidra FUN_1000_0e4b)
// The fight's figure drawer: every figure from its own place, the 'Mechs
// (0-3, 12-15) in view top to bottom, 24x24, sinking in water. (The people,
// figures 4-11 and 16-23: docs/combat.md.)
std::vector<Figure> Town::fight_figures_drawn() const {
    struct Drawn {
        Figure figure;
        int pixel_y;
    };
    std::vector<Drawn> mechs;
    const bool odd_x = position_.x & 1, odd_y = position_.y & 1;
    for (const int f : {0, 1, 2, 3, 12, 13, 14, 15}) {
        const auto fx = figs_.x[std::size_t(f)], fy = figs_.y[std::size_t(f)];
        if (fx == 0xffff) continue;
        const int col = view_col(fx, position_.x), row = view_row(fy, position_.y);
        if ((fx & 0xff00) == (position_.x & 0xff00) && (col < 0x0b || col > 0x27)) continue;
        if ((fy & 0xff00) == (position_.y & 0xff00) && (row < 0 || row > 0x1a)) continue;
        if (col < -0x75 || col > 0xa7 || row < -0x0f80 || row > 0x0f9a) continue;
        // The tile under it: DG:0364/039e give the column's cell.
        int index = view_corner_index(position_) + (row >> 1) * kWindowCells;
        if ((row & 1) && odd_y) index += kWindowCells;
        if (col >= 13) index += (col - 13) >> 1;
        if (col >= 13 && ((col - 13) & 1) && odd_x) index += 1;
        const int quarters = ((row ^ position_.y) & 1) ? 4 : 1;
        Figure fig;
        fig.sprite = sprite((figs_.sprite_base[std::size_t(f)] + animation_.frame(f)) & 0xff);
        fig.half_x = formats::MapPosition{fx, fy}.half_cell_x();
        fig.half_y = formats::MapPosition{fx, fy}.half_cell_y() - 2;
        fig.sink = mech_sink(window_cell(map_, position_, index).value_or(-1), quarters);
        mechs.push_back({fig, 8 * row - 16});
    }
    // Sorted as the original does: for each i, every later one higher on
    // the screen is swapped in.
    for (std::size_t i = 0; i < mechs.size(); ++i)
        for (std::size_t j = i + 1; j < mechs.size(); ++j)
            if (mechs[j].pixel_y < mechs[i].pixel_y) std::swap(mechs[i], mechs[j]);
    std::vector<Figure> out;
    for (const auto& m : mechs) out.push_back(m.figure);
    return out;
}

// BTECH.EXE 24d3:0027..0184 (special 1 of 24d3:0004)
// The training center hands Jason the 'Mech chosen in flag 02 and shows it
// walking in from the west, then runs the mission in flag 00.
bool Town::walk_in(KeySource& keys) {
    if (!rules_ || rules_->mechs.size() < 5) {
        log_.push_back("special 1: no rule tables");
        fight_unfinished_ = true;
        return false;
    }
    // Flag 02: 0 the Locust, 1 the Wasp, 2 the Chameleon (the template at
    // 27e8:04e4); the two last have their own sprites from slot 0x92.
    const int choice = flags_[flag::kTrainingMech];
    const int t = choice == 0 ? 0 : choice == 1 ? 1 : 4;
    figs_.sprite_base[0] = choice == 0 ? 0 : 0x92;
    characters_[0][character::kMech] = 0;
    mechs_[0] = rules_->mechs[std::size_t(t)].record;
    mechs_[0][kPilot] = 0;
    show_picture(0, 0);  // 1000:48b7(0, 0); window 4 stays selected

    // Four half-cell steps east, six frames each.
    position_ = {0x0c3c, 0xc04f};
    animation_.start_script(0, FigureAnimation::mech_script(2));
    std::vector<Figure> last;
    while (position_.x < 0x0c40) {
        animation_.advance(0);  // 1000:1732
        for (int frame = 0; frame < 6; ++frame) {
            if (tiles_animate(map_number_)) phase_ = (phase_ + 1) % 3;  // 1000:240b
            last = walk_in_figures();
            // 273d:06c3 copies the map's columns (13-39) to the screen:
            // whatever text was there is gone.
            text_.forget(13, 0, kTextColumns - 13, kTextRows);
        }
        frozen_ = Frozen{live_view(), last};
        ++position_.x;
    }
    animation_.advance(0);  // a fifth time: frame 6, not drawn here
    return run_mission(flags_[0], keys);
}

// BTECH.EXE 17dc:0629 (Ghidra FUN_17dc_0629), mode 2 (the third training
// mission): one enemy Locust, disarmed, and the fight.
bool Town::run_mission(int mode, KeySource& keys) {
    if (mode != 2) {
        log_.push_back("mission " + std::to_string(mode) + ": not rebuilt");
        fight_unfinished_ = true;
        return false;
    }
    place_mission(1, 0);
    // The target can't shoot back: 'Mech record 4 loses its weapons and
    // ammunition (bytes 0x24, 0x25 and 0x27-0x32; 0x26 stays).
    auto& target = mechs_[4];
    target[0x24] = target[0x25] = 0;
    for (int b = 0x27; b <= 0x32; ++b) target[std::size_t(b)] = 0;
    return fight(1, keys);
}

// BTECH.EXE 17dc:0d49 (Ghidra FUN_17dc_0d49), for a training mission (no
// bit 0x80 in either count): `mechs` enemy Locusts in records 4 on, as
// figures 12 on, and `people` enemy people.
void Town::place_mission(int mechs, int people) {
    figs_.computer[0] = false;
    // The townspeople keep their places in their records; every figure is
    // emptied.
    people_.leave_for_fight();
    for (int m = 4; m < 8; ++m) mechs_[std::size_t(m)][0] = 0xff;
    for (int c = 8; c < 16; ++c) characters_[std::size_t(c)][character::kName] = character::kEmpty;
    for (int f = 0; f < FightFigures::kCount; ++f) {
        figs_.destroyed[std::size_t(f)] = false;
        figs_.active[std::size_t(f)] = 0;
        figs_.x[std::size_t(f)] = figs_.y[std::size_t(f)] = 0xffff;
    }
    figs_.active[0] = 1;
    // Where the enemy starts: two draws, the first thrown away on the
    // course of mission 2, which always starts at x 0c72.
    std::uint16_t x0 = std::uint16_t(0x0c65 + (random_.next() & 7));
    const std::uint16_t y0 = std::uint16_t(0xc059 + (random_.next() & 7));
    if (flags_[0] == 2) x0 = 0x0c72;
    // Flag 03 counts failed tries at the mission: it is kept at 6 at most,
    // and takes armour off the enemy (and, after five, more: below).
    if (flags_[flag::kTries] > 6) flags_[flag::kTries] = 6;
    const int tries = flags_[flag::kTries];
    const int cut = rules_->combat.difficulty_armour[std::size_t(tries)];
    int placed = 0;
    for (int i = 0; i < mechs && i < 4; ++i, ++placed) {
        auto& pilot = characters_[std::size_t(8 + i)];
        pilot[character::kName] = 0;
        pilot[character::kMech] = std::uint8_t(4 + i);
        auto& rec = mechs_[std::size_t(4 + i)];
        rec = rules_->mechs[0].record;
        for (int b = kArmourFirst; b <= kArmourLast; ++b)
            rec[std::size_t(b)] = rec[std::size_t(b)] <= cut ? 0 : std::uint8_t(rec[std::size_t(b)] - cut);
        if (tries > 4)
            for (int n = 0; n < 5; ++n) {
                auto& slot = rec[std::size_t(kSlots + (random_.next() & 0x1f))];
                if (slot != 0) slot |= 0x80;
            }
        if (tries > 5)
            for (const int b : {0x75, 0x77, 0x76}) rec[std::size_t(b)] = std::uint8_t(random_.next() & 1);
        const int f = 12 + i;
        figs_.sprite_base[std::size_t(f)] = 0;
        animation_.set_frame(f, 0x0c);
        animation_.start_script(f, FigureAnimation::mech_script(6));
        animation_.set_direction(f, 6);
        figs_.heading[std::size_t(f)] = 6;
        figs_.active[std::size_t(f)] = 1;
        figs_.x[std::size_t(f)] = std::uint16_t(x0 + tables_.enemy_offset_x[std::size_t(placed)]);
        figs_.y[std::size_t(f)] = std::uint16_t(y0 + tables_.enemy_offset_y[std::size_t(placed)]);
    }
    if (people > 0) log_.push_back("enemy people: not rebuilt");
}

// BTECH.EXE 203b:000a (Ghidra FUN_203b_000a), a fight of kind 1 (a training
// mission) up to its first orders menu: the party's 'Mechs placed around
// the party, then three questions in the party box.
bool Town::fight(int kind, KeySource& keys) {
    const formats::MapPosition at = position_;
    // 2.1: every figure's path ends at once, no targets, no orders; the
    // 'Mechs cool; the computer moves no party figure.
    for (int f = 0; f < CombatState::kFigures; ++f) {
        combat_.paths[std::size_t(f)].fill(2);
        combat_.targets[std::size_t(f)].fill(0xff);
        combat_.orders[std::size_t(f)].fill(0xff);
    }
    combat_.heat.fill(0);
    combat_.burning.fill(0);
    for (int f = 0; f < 12; ++f) figs_.computer[std::size_t(f)] = false;
    // Figures 16-23 share the townspeople's headings (F:3920 + 16 + i).
    for (int i = 0; i < 8; ++i) figs_.heading[std::size_t(16 + i)] = people_.walkers()[std::size_t(i)].heading;
    // Each 'Mech of records 0-3 at the k-th 'Mech place (the walk-in's).
    int k = 0;
    for (int m = 0; m < 4; ++m) {
        if (mechs_[std::size_t(m)][0] == 0xff) continue;
        figs_.x[std::size_t(k)] = move_x(at.x, tables_.mech_formation_x[std::size_t(k)]);
        figs_.y[std::size_t(k)] = move_y(at.y, tables_.mech_formation_y[std::size_t(k)]);
        ++figs_.active[std::size_t(k)];
        ++k;
    }
    for (int c = 0; c < 8; ++c)
        if (on_foot(c)) log_.push_back("a character on foot in a fight: not rebuilt");

    // 15ab:0b5e: "Do you want the computer to fight for you?"
    text_.select_window(3);
    text_.clear_window();
    party_box_shown_ = false;
    print_id("fight.computer_question");
    const auto computer = yes_no(computer_, keys);
    if (!computer) {
        place_open_ = true;
        return false;
    }
    computer_ = *computer;
    // 203b:24f0: the combat messages, a menu of three (menu record 3 from
    // row 2, starting on the last choice).
    text_.select_window(3);
    text_.clear_window();
    print_id("options.messages");
    menus_[3].first_row = 2;
    menus_[3].items = 3;
    menu_last_[3] = messages_;
    const auto messages = menu(3, keys);
    if (!messages) return false;
    menus_[3].first_row = 0;
    messages_ = *messages;
    // 203b:2556: "See combat graphics:"
    text_.select_window(3);
    text_.clear_window();
    print_id("options.graphics");
    const auto graphics = yes_no(graphics_, keys);
    if (!graphics) {
        place_open_ = true;
        return false;
    }
    graphics_ = *graphics;
    if (computer_) return fight_rounds(kind, keys);
    // The round (203b:056b): units without a place drop out.
    for (int f = 0; f < 12; ++f)
        if (figs_.active[std::size_t(f)] != 0 && (figs_.x[std::size_t(f)] == 0xffff || figs_.y[std::size_t(f)] == 0xffff))
            figs_.active[std::size_t(f)] = 0;
    return orders_menu(keys);
}

// BTECH.EXE 15ab:18e8 (Ghidra FUN_15ab_18e8), for a 'Mech: its pilot's
// name, "'s" and a new line, and the 'Mech's name, in the upper box.
void Town::print_unit_heading(int fig) {
    text_.set_fg(15);
    const auto& rec = mechs_[std::size_t(fig)];
    const auto& pilot = characters_[std::size_t(rec[kPilot])];
    print_id("character." + std::to_string(pilot[character::kName]));
    print_id("unit.possessive");
    // The 'Mech's name: the text of the template the record was made from.
    const auto ids = formats::mech_template_ids();
    for (std::size_t t = 0; t < rules_->mechs.size() && t < ids.size(); ++t)
        if (std::equal(rec.begin(), rec.begin() + formats::kMechNameSize, rules_->mechs[t].record.begin())) {
            print_id(formats::mech_text_id(ids[t]));
            return;
        }
    log_.push_back("a 'Mech record with no template's name");
}

// BTECH.EXE 203b:14c3 (Ghidra FUN_203b_14c3), up to its menu: the first
// active unit's heading in the upper box, the view moved onto it and
// drawn with the fight's figures and a box on the unit, and the orders
// menu with the bar on Begin Fight.
bool Town::orders_menu(KeySource& keys) {
    int fig = -1;
    for (int f = 0; f < 12 && fig < 0; ++f)
        if (figs_.active[std::size_t(f)] != 0) fig = f;
    if (fig < 0 || fig >= 4) {
        log_.push_back("orders for a unit on foot: not rebuilt");
        fight_unfinished_ = true;
        return false;
    }
    text_.select_window(4);
    text_.clear_window();
    print_unit_heading(fig);
    // 203b:1774: 1000:17bb walks the party's position onto the unit, the
    // view is drawn there (287f:1314, 18ef), the figures (1000:0e4b), the
    // map columns copied to the screen (273d:06c3), and a 3x3-cell box
    // XORed in colour 15 over the 'Mech in the middle of the view.
    position_ = {figs_.x[std::size_t(fig)], figs_.y[std::size_t(fig)]};
    frozen_ = Frozen{live_view(), fight_figures_drawn()};
    text_.forget(13, 0, kTextColumns - 13, kTextRows);
    boxes_ = {XorBox{25, 10, 3, 3, 15}};
    // The orders (DG:3a6f), menu record 3 with ten items, the bar on the
    // last (Begin Fight) each time.
    text_.select_window(3);
    text_.clear_window();
    text_.set_fg(15);
    print_id("orders.menu_mech");
    menus_[3].items = 10;
    menu_last_[3] = 9;
    const auto order = menu(3, keys);
    if (!order) return false;
    log_.push_back("order " + std::to_string(*order) + ": not rebuilt");
    fight_unfinished_ = true;
    return false;
}

Combat Town::combat_rules() {
    Combat::Hooks hooks;
    hooks.print = [this](const std::string& id) { print_id(id); };
    hooks.clear_box = [this] {
        text_.select_window(4);
        text_.clear_window();
    };
    hooks.log = [this](const std::string& line) {
        if (log_.empty() || log_.back() != line) log_.push_back(line);
    };
    return Combat(map_, blocking_tile(map_number_), position_, figs_, animation_, mechs_, characters_, flags_,
                  random_, combat_, marks_, *rules_, tables_, fixes_, messages_, graphics_, std::move(hooks));
}

// BTECH.EXE 203b:056b..1481, the rounds with the computer in control
// (docs/combat.md 2.5, 2.8, 2.12, 2.13): with combat messages None and
// combat graphics off; with either, the fight stops here.
bool Town::fight_rounds(int kind, KeySource& keys) {
    if (messages_ != 0 || graphics_) {
        log_.push_back("a fight with combat messages or graphics: not rebuilt");
        fight_unfinished_ = true;
        return false;
    }
    Combat combat = combat_rules();
    bool over = false;
    while (!over) {
        const formats::MapPosition start = position_;
        combat_.person_ran = false;
        // 203b:1482(0): the computer plans the party.
        combat.plan_side(0);
        if (combat_.person_ran) {
            // A party person met a 'Mech: with no party 'Mech left, the
            // party may run (r & 3).
            bool no_mech = true;
            for (int m = 0; m < 4; ++m)
                if (mechs_[std::size_t(m)][0] != 0xff) no_mech = false;
            if (no_mech && (random_.next() & 3) != 0) over = true;
        }
        if (!over) {
            position_ = start;
            combat_.enemy_people_ran = false;
            combat.plan_side(12);
            position_ = start;
            combat.round(kind > 0);
            if (combat_.fight_on && flags_[0x27] != 0) log_.push_back("the double agent: not rebuilt");
            // 2.8: the end tests.
            bool exit_zone = false;
            if (flags_[0x23] != 0 && figs_.active[0] != 0 && figs_.x[0] > 0x0900 && figs_.x[0] < 0x0a07)
                exit_zone = true;
            over = true;
            for (int f = 12; f < 24; ++f) {
                if (figs_.x[std::size_t(f)] == 0xffff || figs_.y[std::size_t(f)] == 0xffff)
                    figs_.active[std::size_t(f)] = 0;
                if (figs_.active[std::size_t(f)] != 0) over = false;
            }
            if (!over && combat_.enemy_people_ran && kind == 0) log_.push_back("enemy people running: not rebuilt");
            if (kind == 1 && figs_.active[0] == 0) over = true;
            if (!exit_zone && kind == 2 && figs_.active[0] == 0 && flags_[0x23] == 0) over = true;
            if (exit_zone) over = true;
            if (!combat_.fight_on) over = true;
        }
        // With the computer in control a waiting key hands the party back
        // to the player (273d:002f, no demo playing), and is thrown away.
        const auto waiting = keys.poll();
        if (!waiting) {
            place_open_ = true;  // the keys ran out between rounds
            return false;
        }
        if (*waiting) {
            computer_ = false;
            if (!over) {
                log_.push_back("the player takes over the fight: not rebuilt");
                fight_unfinished_ = true;
                return false;
            }
        }
    }

    // After the loop (203b:1040): window 3 is cleared.
    text_.select_window(3);
    text_.clear_window();
    if (kind == 1 && combat_.fight_on) {
        // 24d3:17c6 / 17ea and a key.
        open_message_box();
        print_id("fight.return_to_base");
        if (!read_key(keys)) {
            place_open_ = true;
            return false;
        }
    } else if (kind != 1) {
        log_.push_back("the ending of a fight of kind " + std::to_string(kind) + ": not rebuilt");
        fight_unfinished_ = true;
        return false;
    }
    // 2.13: the townspeople are put back; F:396c of figures 0-7 cleared;
    // the townspeople's frame 0x10.
    std::array<std::int8_t, 8> headings{};
    for (int i = 0; i < 8; ++i) headings[std::size_t(i)] = figs_.heading[std::size_t(16 + i)];
    people_.back_from_fight(headings);
    for (int i = 0; i < 8; ++i) {
        figs_.x[std::size_t(16 + i)] = people_.walkers()[std::size_t(i)].at.x;
        figs_.y[std::size_t(16 + i)] = people_.walkers()[std::size_t(i)].at.y;
        animation_.set_direction(i, -1);
        animation_.set_frame(16 + i, 0x10);
    }
    log_.push_back("the fight is over: what follows it is not rebuilt");
    fight_unfinished_ = true;
    return false;
}

}  // namespace chr::game
