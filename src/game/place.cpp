// Inside a place: the entrance routine, the .BLD script interpreter and the
// special actions the Citadel uses (docs/scripts.md). The script's bytes come
// from the player's .BLD at run time; every text it prints is looked up by
// its text ID (bld.<file>.<offset>), so the words come from the import.

#include <cstdio>
#include <stdexcept>

#include "formats/bld.h"
#include "formats/game_files.h"
#include "game/movement.h"
#include "game/town.h"
#include "game/training_quiz.h"

namespace chr::game {

namespace {

constexpr int kPanelPictureWindow = 4;

}  // namespace

std::optional<int> Town::read_key(KeySource& keys) {
    const auto k = keys.next();
    if (!k) place_open_ = true;
    return k;
}

void Town::print_id(const std::string& id) { text_.print(lookup(texts_, id)); }

// The bars are drawn into window 3, the place's picture into window 4:
// clearing the window takes them away too.
void Town::clear_window() {
    text_.clear_window();
    if (text_.window_number() == 3) party_box_shown_ = false;
    if (text_.window_number() == kPanelPictureWindow) picture_.reset();
}

// BTECH.EXE 17dc:0008 (Ghidra FUN_17dc_0008)
// Goes into the place behind entrance `entrance`: runs its script, then puts
// the panel back. The step that offered it goes on afterwards as after "no".
bool Town::visit_place(int entrance, KeySource& keys) {
    flags_[flag::kInPlace] = 1;
    place_number_ = entrance;
    int file = entrance;
    if (entrance != 0x12 && entrance < 0x16) file = map_.place_scripts[std::size_t(entrance)];
    // 1000:28cc asks for the game disk; there are no disks. Entrance 0 first
    // runs the training center's quiz, 1c67:08a8, while flag 14 is clear and
    // no demo plays (F:3938, F:458c). It has no check here (docs/rules.md,
    // rule 3; docs/combat.md 7): its draws are made, and with --show-quiz its
    // scene is shown.
    if (entrance == 0 && flags_[flag::kQuizLock] == 0) {
        if (show_quiz_ && rules_) {
            if (!show_quiz(keys)) return false;
        } else {
            const auto parts = draw_quiz_parts(random_);
            log_.push_back("training quiz: parts " + std::to_string(parts[0]) + " " + std::to_string(parts[1]) +
                           " " + std::to_string(parts[2]) + " (not shown)");
        }
        // 1c67:0b40: the panel, the party box cleared, then the box above it.
        draw_panel(true);
        text_.clear_window();
    }
    if (!run_script(file, keys)) return false;
    // Special 14 after every visit.
    count_members();
    text_.select_window(kPanelPictureWindow);
    text_.clear_window();
    picture_.reset();
    text_.select_window(3);
    draw_panel();
    return true;
}

// BTECH.EXE 17dc:01c0 (Ghidra FUN_17dc_01c0)
// Loads script `file` (17dc:1d30) and runs it from offset 0 to its `end`.
bool Town::run_script(int file, KeySource& keys) {
    const std::string name(formats::kBldFiles[std::size_t(file)]);
    const auto path = formats::find_game_file(*game_dir_, name + ".BLD");
    if (!path) throw std::runtime_error(name + ".BLD is missing from " + game_dir_->string());
    const auto script = formats::read_bld(*path);
    // DGROUP:141a, the picture by script number, was part of BTECH.EXE; it is
    // now imported (tables.json: place_pictures).
    if (const int picture = tables_.place_pictures[std::size_t(file)]; picture != 0) show_picture(picture, 1);
    log_.push_back("script " + name);

    int pc = 0;
    const int size = static_cast<int>(script.size());
    auto sbyte = [&](int at) { return static_cast<int>(static_cast<std::int8_t>(script.at(std::size_t(at)))); };
    while (pc < size) {
        const auto in = formats::decode(script, pc, menus_);
        if (!in.op) {  // not an opcode: stepped over
            pc = in.next;
            continue;
        }
        int next = in.next;
        auto jump_if = [&](bool taken) {
            if (taken) next = in.targets.back();
        };
        switch (*in.op) {
        case formats::Op::End:
            return true;
        case formats::Op::Window:
            text_.select_window(sbyte(pc + 1));
            break;
        case formats::Op::Clear:
            clear_window();
            break;
        case formats::Op::Frame:
            text_.frame(sbyte(pc + 1));
            break;
        case formats::Op::Print:
            print_id(formats::bld_text_id(name, pc));
            break;
        case formats::Op::WaitKey:
            if (!read_key(keys)) return false;
            break;
        case formats::Op::Pause:
            // 50 ticks (273d:0006), then the keyboard is emptied (1000:2a2b):
            // a key typed ahead is lost. Keys here only come when asked for.
            break;
        case formats::Op::Goto:
            next = in.targets.back();
            break;
        case formats::Op::Menu: {
            const auto chosen = menu(sbyte(pc + 1), keys);
            if (!chosen) return false;
            next = in.targets.at(std::size_t(*chosen));
            break;
        }
        case formats::Op::SwitchFlag: {
            // The table is indexed by the flag as a signed byte; the program
            // reads whatever word is there.
            const int value = static_cast<std::int8_t>(flags_[std::size_t(in.bytes[0])]);
            next = script.at(std::size_t(pc + 2 + 2 * value)) | script.at(std::size_t(pc + 3 + 2 * value)) << 8;
            break;
        }
        case formats::Op::IfFlag:
            jump_if(flags_[std::size_t(in.bytes[0])] != 0);
            break;
        case formats::Op::SetFlag:
            flags_[std::size_t(in.bytes[0])] = static_cast<std::uint8_t>(in.bytes[1]);
            break;
        case formats::Op::AddFlag:
            flags_[std::size_t(in.bytes[0])] = static_cast<std::uint8_t>(flags_[std::size_t(in.bytes[0])] + in.bytes[1]);
            break;
        case formats::Op::Cursor:
            text_.set_cursor(sbyte(pc + 1), sbyte(pc + 2));
            break;
        case formats::Op::IfYes: {
            const auto yes = yes_no(true, keys);
            if (!yes) {
                place_open_ = true;
                return false;
            }
            jump_if(*yes);
            break;
        }
        case formats::Op::Special:
            if (!special(in.bytes[0], keys)) return false;
            break;
        case formats::Op::Earn:
            money_.cbills += static_cast<std::int16_t>(in.words[0]);
            draw_cbills();
            break;
        case formats::Op::Pay: {
            const std::int64_t amount = static_cast<std::int16_t>(in.words[0]);
            // Compared as 32-bit unsigned numbers: more than they have takes it to 0.
            const auto have = static_cast<std::uint32_t>(money_.cbills);
            const auto take = static_cast<std::uint32_t>(amount);
            money_.cbills = take > have ? 0 : static_cast<std::int32_t>(have - take);
            draw_cbills();
            break;
        }
        case formats::Op::IfCBills: {
            const auto need = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(in.words[0])));
            jump_if(static_cast<std::uint32_t>(money_.cbills) >= need);
            break;
        }
        case formats::Op::IfFlagD450:
        case formats::Op::IfFlagD451:
            // F:d450 / d451 (docs/scripts.md, "Not rebuilt yet").
            jump_if(false);
            break;
        case formats::Op::IfRandom:
            jump_if((random_.next() & sbyte(pc + 1)) != 0);
            break;
        case formats::Op::IfPartyX:
            jump_if(in.words[0] == position_.x);
            break;
        case formats::Op::SetPartyPlace:
            position_ = {static_cast<std::uint16_t>(in.words[0]), static_cast<std::uint16_t>(in.words[1])};
            break;
        case formats::Op::IfSkill: {
            bool any = false;
            for (std::size_t i = 0; i < 8; ++i)
                if (party().present[i] && in.bytes[0] < character::kSkillCount && characters_[i][std::size_t(character::kSkills + in.bytes[0])] >= sbyte(pc + 2)) any = true;
            jump_if(any);
            break;
        }
        case formats::Op::Picture:
            show_picture(sbyte(pc + 1), sbyte(pc + 2));
            break;
        case formats::Op::Sound:
            // 1000:19bf: a sound effect, with sound on (docs/scripts.md).
            break;
        case formats::Op::NewMember:
            // 19b8:0d58: a new member joins (docs/scripts.md).
            log_.push_back("new member: not rebuilt");
            break;
        }
        pc = next;
    }
    return true;
}

// BTECH.EXE 2656:0b5e (Ghidra FUN_2656_0b5e)
// Menu m in the current window: a bar on the item it last chose, moved by
// the arrows (wrapping), Enter or space to choose.
std::optional<int> Town::menu(int m, KeySource& keys, std::optional<int> start) {
    const auto& rec = menus_.at(std::size_t(m));
    int& last = menu_last_.at(std::size_t(m));
    if (last >= rec.items) last = 0;
    int chosen = start && *start >= 0 && *start < rec.items ? *start : last;
    const auto& w = text_.window();
    const int top = w.y + rec.first_row;
    text_.xor_bar(w.x, top + chosen, rec.width, rec.highlight);
    for (;;) {
        const auto k = read_key(keys);
        if (!k) return std::nullopt;
        const int a = arrow_key(*k);
        if (a == key::kEnter || a == key::kSpace) break;
        if (a != key::kUp && a != key::kDown) continue;
        text_.xor_bar(w.x, top + chosen, rec.width, rec.highlight);
        chosen += a == key::kUp ? -1 : 1;
        if (chosen < 0) chosen = rec.items - 1;
        if (chosen >= rec.items) chosen = 0;
        text_.xor_bar(w.x, top + chosen, rec.width, rec.highlight);
    }
    // (The program would return the old item for Esc without remembering
    // it, but only Enter and space end the loop.)
    last = chosen;
    return chosen;
}

// BTECH.EXE 24d3:0004 (Ghidra FUN_24d3_0004), the cases the Citadel uses.
bool Town::special(int n, KeySource& keys) {
    switch (n) {
    case 1:
        return walk_in(keys);  // game/fight.cpp
    case 2:
        // 24d3:0187: after the course, by the 'Mech's first letter.
        if (mechs_[0][0] == 'L') {
            print_id("course.result_locust");
        } else {
            print_id(mechs_[0][0] == 'W' ? "course.result_wasp" : "course.result_chameleon");
            print_id("course.result_other");
        }
        return true;
    case 3:
        // 24d3:01c2: the 'Mech is handed back. Jason is on foot outside the
        // training center; unless flag 04 is set, the fires (marks 0x7c and
        // 0x7d) are taken off the map, all but their y.
        mechs_[0][0] = 0xff;
        characters_[0][character::kMech] = character::kOnFoot;
        position_ = {0x0c2e, 0xc076};
        if (flags_[flag::kOutdoors] == 0)
            for (auto& m : marks_.marks)
                if ((m.code & 0x7e) == 0x7c) m.code = m.area = m.x = 0;
        return true;
    case 4:
        // 24d3:0231: the three courses' prices, 125 x Jason's skill + 75,
        // right-aligned to column 39 on rows 5-7, light green.
        for (int i = 0; i < 3; ++i) {
            const std::string price = std::to_string(125 * characters_[0][std::size_t(character::kSkills + i)] + 75);
            text_.put_text(price, 39 - static_cast<int>(price.size()), 5 + i, 10, 0);
        }
        return true;
    case 5: {
        // 24d3:02a5: Jason takes the course in flag 08.
        const int course = static_cast<std::int8_t>(flags_[flag::kCourse]);
        const int level = characters_[0].at(std::size_t(character::kSkills + course));
        print_id("course.transcript");
        print_id("course.level." + std::to_string(level));
        print_id("course.subject." + std::to_string(course));
        if (level == 4) {
            print_id("course.maxed");
            flags_[flag::kEnrolled] = 0;
            return true;
        }
        const int price = 125 * level + 75;
        if (static_cast<std::uint32_t>(money_.cbills) >= static_cast<std::uint32_t>(price)) {
            flags_[flag::kEnrolled] = 1;
            ++characters_[0][std::size_t(character::kSkills + course)];
            money_.cbills -= price;
            draw_cbills();  // 24d3:1961, the end of several cases
        } else {
            print_id("course.no_money");
            print_id("course.no_money_end");  // 24d3:1809
            flags_[flag::kEnrolled] = 0;
        }
        return true;
    }
    case 43: {
        // 24d3:157e: Rick Atlas waits in the Lounge. Townsperson 0 stands
        // at the Lounge's entrance (7), inside (wait ff), last in the
        // Lounge and bound for the Training Center (entrance 0), until the
        // talk lets him out (flag 2d, talk()).
        auto& w = people_.walkers()[0];
        w.at = map_.entrances[7];
        w.target = map_.entrances[0];
        w.route = 0x70;
        w.wait = 0xff;
        figs_.x[16] = w.at.x;
        figs_.y[16] = w.at.y;
        return true;
    }
    case 12:
        return talk(keys);
    case 14:
        count_members();
        return true;
    case 23:
        flags_[0x19] = anyone_hurt() ? 1 : 0;
        return true;
    case 17: {
        // 24d3:0ca4: flag 10 = how many of 'Mech records 0-3 are in use.
        int in_use = 0;
        for (const bool e : mech_records()) in_use += e ? 1 : 0;
        flags_[0x10] = std::uint8_t(in_use);
        return true;
    }
    default:
        log_.push_back("special " + std::to_string(n) + ": not rebuilt");
        return true;
    }
}

// BTECH.EXE 24d3:0cf8, special 23: flag 19 = whether any party member's
// hit points are below 10 x B.
bool Town::anyone_hurt() const {
    for (int c = 0; c < 8; ++c) {
        const auto& r = characters_[std::size_t(c)];
        if (r[character::kName] == character::kEmpty) continue;
        if (std::int8_t(r[character::kHitPoints]) != std::int8_t(r[character::kB]) * 10) return true;
    }
    return false;
}

// BTECH.EXE 24d3:0af2: flag 0e = the members after Jason.
void Town::count_members() {
    int count = 0;
    for (std::size_t i = 1; i < 8; ++i)
        if (party().present[i]) ++count;
    flags_[flag::kMembers] = static_cast<std::uint8_t>(count);
}

// BTECH.EXE 17dc:17b9 (Ghidra FUN_17dc_17b9)
// "Talk to others": the townspeople inside this place (in for some rounds,
// last gone in at its entrance) are listed in a menu; the one chosen says
// where it goes next.
bool Town::talk(KeySource& keys) {
    // The entrance whose script is the one in flag 0d.
    int here = 0;
    while (here < 8 && map_.place_scripts[std::size_t(here)] != flags_[flag::kPlace]) ++here;
    text_.clear_window();
    std::array<bool, 8> inside{};
    int count = 0;
    const auto& walkers = people_.walkers();
    for (std::size_t i = 0; i < 8; ++i) {
        if (walkers[i].wait != 0 && (walkers[i].route >> 4 & 7) == here) {
            inside[i] = true;
            ++count;
        }
    }
    if (count == 0) {
        print_id("talk.nobody");
        return read_key(keys).has_value();
    }
    print_id("talk.you_see");
    for (std::size_t i = 0; i < 8; ++i) {
        if (!inside[i]) continue;
        print_id("map" + std::to_string(map_number_) + ".person." + std::to_string(i));
        print_id("talk.line_end");  // 24d3:181e
    }
    print_id("talk.done");
    menus_[20].items = count + 1;
    const auto chosen = menu(20, keys);
    if (!chosen) return false;
    if (*chosen == count) return true;
    if (flags_[flag::kLetOut] != 0 && here == 7 && *chosen == 0 && inside[0]) {
        // Someone is let out of jail (the second map's story).
        flags_[flag::kLetOutDone] = 1;
        people_.walkers()[0].wait = 0;
        return true;
    }
    flags_[flag::kLetOutDone] = 0;
    text_.clear_window();
    if (flags_[flag::kOutdoors] != 0) {
        // Outdoors: one of eight rumours (DGROUP:4e5e) at random (docs/scripts.md).
        random_.next();
        log_.push_back("talk outdoors: not rebuilt");
        return read_key(keys).has_value();
    }
    print_id("talk.hello");
    print_id("talk.reason." + std::to_string(static_cast<std::int8_t>(flags_[flag::kPlace])));
    print_id("talk.going_to");
    int left = *chosen;
    for (std::size_t i = 0; i < 8; ++i) {
        if (!inside[i] || --left >= 0) continue;
        print_id(place_text_id(map_number_, walkers[i].route & 7));
        print_id("talk.end");
        return read_key(keys).has_value();
    }
    return true;
}

// BTECH.EXE 1000:48b7 (Ghidra FUN_1000_48b7)
// A place's picture in the upper left box. One random number is always
// drawn; unless the place always shows its picture, it shows only when
// that number AND the chance mask (by flag 4f) is 0.
void Town::show_picture(int number, int mode) {
    const int r = random_.next();
    const int chance = flags_[flag::kPictureChance];
    // DGROUP:0ac4, the chance masks, was part of BTECH.EXE; it is now
    // imported (tables.json: picture_chance).
    const auto& masks = tables_.picture_chance;
    bool show = chance >= 0 && chance < static_cast<int>(masks.size()) && (r & masks[std::size_t(chance)]) == 0;
    if (number != 0x10 && (number == 0 || number > 7 || number == 6 || number == 2)) show = true;
    if (mode == 2) show = true;
    if (!show) return;
    // The frames play with their delays (formats/anm.h); the picture then
    // stays as its last frame shows it. Mode 0 (a story picture) waits 50
    // frames after it (none for picture 8) and clears the box again.
    if (mode == 0) {
        if (number != 0) {
            picture_ = PlacePicture{number};
            hold_screen(number == 8 ? 0 : 50);
        }
        text_.select_window(kPanelPictureWindow);
        text_.clear_window();
        picture_.reset();
        return;
    }
    text_.forget(1, 1, 11, 11);
    picture_ = PlacePicture{number};
}

}  // namespace chr::game
