#include "formats/exe_data.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "formats/cmp.h"  // FormatError
#include "formats/exepack.h"
#include "formats/game_files.h"
#include "formats/sha256.h"

namespace chr::formats {

namespace {

// DOS 1.03. DGROUP (the program's data segment) is 46db; the asm display
// library's data is 2c6c. Text pieces keep the program's control bytes
// (docs/text.md); several of them are not NUL-free, so each has its length.
constexpr std::array kDos103 = {
    // 2c6c:a661, 1,024 bytes: the 8x8 font, one bit per pixel, 8 bytes per
    // character, characters 0..127 (273d:00d5 masks with 0x7f).
    ExeSpan{ExeSpan::Kind::Font, "font", 0x2c6c, 0xa661, 0x400},
    // The rule tables (formats/rules.h, docs/formats/mech.md), written to
    // rules/*.json. The eight 'Mech templates, 0x7d bytes each, that
    // 17dc:0d49 and the new game (24d3) copy into the 'Mech records:
    ExeSpan{ExeSpan::Kind::Table, "rules.mech_templates", 0x37e8, 0x02f0, 8 * 0x7d},
    // The weapon table, 0x21 records of 0x11 bytes (22e8:000c, 1e31:0f24):
    ExeSpan{ExeSpan::Kind::Table, "rules.weapons", 0x46db, 0x2ed8, 0x21 * 0x11},
    // 22e8:000c's hit location table, DG:2e40 + 11 * side + 2d6 (4 x 11):
    ExeSpan{ExeSpan::Kind::Table, "rules.hit_locations", 0x46db, 0x2e42, 44},
    // 22e8:000c's cluster table, DG:2e5e + 7 * 2d6 + column (11 x 7):
    ExeSpan{ExeSpan::Kind::Table, "rules.clusters", 0x46db, 0x2e6e, 77},
    // The side hit by facing difference, DG:2d11 + (-7..7):
    ExeSpan{ExeSpan::Kind::Table, "rules.facing_sides", 0x46db, 0x2d0a, 15},
    // The to-hit modifier by the target's steps so far, DG:2d1a + 0..12:
    ExeSpan{ExeSpan::Kind::Table, "rules.target_movement", 0x46db, 0x2d1a, 13},
    // 17dc:0d49's armour cut by difficulty, DG:165a + 0..6:
    ExeSpan{ExeSpan::Kind::Table, "rules.difficulty_armour", 0x46db, 0x165a, 7},
    // The personal armour types' points, DG:4ddb + 0..5 (1000:3a49, 15ab:1a8f, 24d3:0a3c):
    ExeSpan{ExeSpan::Kind::Table, "rules.personal_armour", 0x46db, 0x4ddb, 6},
    // Text the game shows. The names are the text IDs (docs/text.md): a
    // text provider hands them out, and an override file can replace them.
    // 1000:1c12, the step check:
    ExeSpan{ExeSpan::Kind::Text, "walk.too_deep", 0x46db, 0x0410, 32},
    ExeSpan{ExeSpan::Kind::Text, "walk.enter_question", 0x46db, 0x0430, 20},  // then the place's name
    ExeSpan{ExeSpan::Kind::Text, "walk.enter_question_end", 0x46db, 0x0444, 2},
    // 1000:00bb, the main loop: after a walking key it writes the direction
    // in the upper panel box (pointers at DGROUP:01aa, in the order of the
    // keys at DGROUP:0160). Each starts with a new line.
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.north", 0x46db, 0x006c, 7},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.northeast", 0x46db, 0x0073, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.east", 0x46db, 0x007e, 6},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.southeast", 0x46db, 0x0084, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.south", 0x46db, 0x008f, 7},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.southwest", 0x46db, 0x0096, 11},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.west", 0x46db, 0x00a1, 6},
    ExeSpan{ExeSpan::Kind::Text, "walk.direction.northwest", 0x46db, 0x00a7, 11},
    // 1000:1a13, the yes/no answer line, with Yes or with No picked:
    ExeSpan{ExeSpan::Kind::Text, "ui.answer_yes_picked", 0x46db, 0x03ee, 16},
    ExeSpan{ExeSpan::Kind::Text, "ui.answer_no_picked", 0x46db, 0x03fe, 18},
    // 1000:4cac, the heading over the party box's three bar columns:
    ExeSpan{ExeSpan::Kind::Text, "panel.party_heading", 0x46db, 0x0aca, 4},
    ExeSpan{ExeSpan::Kind::Text, "panel.cbills_label", 0x46db, 0x32da, 12},
    // The eight characters' names (pointers at DGROUP:01ca):
    ExeSpan{ExeSpan::Kind::Text, "character.0", 0x46db, 0x00b2, 6},
    ExeSpan{ExeSpan::Kind::Text, "character.1", 0x46db, 0x00b8, 4},
    ExeSpan{ExeSpan::Kind::Text, "character.2", 0x46db, 0x00bc, 7},
    ExeSpan{ExeSpan::Kind::Text, "character.3", 0x46db, 0x00c3, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.4", 0x46db, 0x00c8, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.5", 0x46db, 0x00cd, 5},
    ExeSpan{ExeSpan::Kind::Text, "character.6", 0x46db, 0x00d2, 7},
    ExeSpan{ExeSpan::Kind::Text, "character.7", 0x46db, 0x00d9, 6},
    // The places' special actions (docs/scripts.md). The combat class
    // (24d3:02a5): the transcript, the five levels (pointers at DGROUP:4ea2)
    // and the three subjects (4eb6).
    ExeSpan{ExeSpan::Kind::Text, "course.transcript", 0x46db, 0x491a, 46},
    ExeSpan{ExeSpan::Kind::Text, "course.level.0", 0x46db, 0x46dc, 21},
    ExeSpan{ExeSpan::Kind::Text, "course.level.1", 0x46db, 0x46f1, 11},
    ExeSpan{ExeSpan::Kind::Text, "course.level.2", 0x46db, 0x46fc, 10},
    ExeSpan{ExeSpan::Kind::Text, "course.level.3", 0x46db, 0x4706, 7},
    ExeSpan{ExeSpan::Kind::Text, "course.level.4", 0x46db, 0x470d, 15},
    ExeSpan{ExeSpan::Kind::Text, "course.subject.0", 0x46db, 0x471c, 34},
    ExeSpan{ExeSpan::Kind::Text, "course.subject.1", 0x46db, 0x473e, 42},
    ExeSpan{ExeSpan::Kind::Text, "course.subject.2", 0x46db, 0x4768, 34},
    ExeSpan{ExeSpan::Kind::Text, "course.maxed", 0x46db, 0x4948, 81},
    ExeSpan{ExeSpan::Kind::Text, "course.no_money", 0x46db, 0x4999, 58},
    ExeSpan{ExeSpan::Kind::Text, "course.no_money_end", 0x46db, 0x4f7e, 34},
    // The start-up (1000:50c8): the copyright notice and the first-time
    // question, then, after "No", the question about a saved game.
    ExeSpan{ExeSpan::Kind::Text, "intro.notice.0", 0x46db, 0x0b0e, 62},
    ExeSpan{ExeSpan::Kind::Text, "intro.notice.1", 0x46db, 0x0b4c, 68},
    ExeSpan{ExeSpan::Kind::Text, "intro.notice.2", 0x46db, 0x0b90, 82},
    ExeSpan{ExeSpan::Kind::Text, "intro.notice.3", 0x46db, 0x0be2, 43},
    ExeSpan{ExeSpan::Kind::Text, "intro.first_time", 0x46db, 0x0c0d, 44},
    ExeSpan{ExeSpan::Kind::Text, "intro.load_old_game", 0x46db, 0x0c39, 95},
    // Talking to the townspeople inside (17dc:17b9); the reasons are by
    // place (pointers at DGROUP:4e32).
    ExeSpan{ExeSpan::Kind::Text, "talk.nobody", 0x46db, 0x16de, 48},
    ExeSpan{ExeSpan::Kind::Text, "talk.you_see", 0x46db, 0x170e, 26},
    ExeSpan{ExeSpan::Kind::Text, "talk.done", 0x46db, 0x1728, 5},
    ExeSpan{ExeSpan::Kind::Text, "talk.hello", 0x46db, 0x172d, 52},
    ExeSpan{ExeSpan::Kind::Text, "talk.going_to", 0x46db, 0x1761, 46},
    ExeSpan{ExeSpan::Kind::Text, "talk.end", 0x46db, 0x178f, 2},
    ExeSpan{ExeSpan::Kind::Text, "talk.line_end", 0x46db, 0x4fa0, 2},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.0", 0x46db, 0x4375, 89},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.1", 0x46db, 0x43ce, 51},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.2", 0x46db, 0x4401, 78},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.3", 0x46db, 0x444f, 18},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.4", 0x46db, 0x4461, 32},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.5", 0x46db, 0x4481, 56},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.6", 0x46db, 0x44b9, 22},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.7", 0x46db, 0x44cf, 8},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.8", 0x46db, 0x44d7, 1},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.9", 0x46db, 0x44d8, 14},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.10", 0x46db, 0x44e6, 30},
    ExeSpan{ExeSpan::Kind::Text, "talk.reason.11", 0x46db, 0x4504, 29},
    // Combat (docs/combat.md), by the function that prints them. Pieces that
    // a function strings together have their own IDs; a translation keeps
    // the pieces' order, or overrides them all.
    // 15ab:0002, repairs from the wrecks and scrap money:
    ExeSpan{ExeSpan::Kind::Text, "after.repair.break", 0x46db, 0x0efb, 3},
    ExeSpan{ExeSpan::Kind::Text, "after.repair.scavenges", 0x46db, 0x0efe, 42},
    ExeSpan{ExeSpan::Kind::Text, "after.repair.from_wrecks", 0x46db, 0x0f28, 24},
    ExeSpan{ExeSpan::Kind::Text, "after.scrap.found", 0x46db, 0x0f40, 37},
    ExeSpan{ExeSpan::Kind::Text, "after.scrap.cbills", 0x46db, 0x0f65, 57},
    ExeSpan{ExeSpan::Kind::Text, "after.scrap.plural", 0x46db, 0x0f9e, 2},
    ExeSpan{ExeSpan::Kind::Text, "after.scrap.end", 0x46db, 0x0fa0, 2},
    // 15ab:04f9, salvaging a wreck for a pilot without a 'Mech:
    ExeSpan{ExeSpan::Kind::Text, "after.salvage.question", 0x46db, 0x0fae, 81},
    ExeSpan{ExeSpan::Kind::Text, "after.salvage.inspecting", 0x46db, 0x0fff, 30},
    ExeSpan{ExeSpan::Kind::Text, "after.salvage.salvaging", 0x46db, 0x101d, 18},
    ExeSpan{ExeSpan::Kind::Text, "after.salvage.no_chance", 0x46db, 0x102f, 29},
    ExeSpan{ExeSpan::Kind::Text, "after.salvage.none_left", 0x46db, 0x104c, 57},
    // 15ab:094b, looting the beaten soldiers:
    ExeSpan{ExeSpan::Kind::Text, "after.loot.pockets", 0x46db, 0x1085, 48},
    ExeSpan{ExeSpan::Kind::Text, "after.loot.cash", 0x46db, 0x10b5, 17},
    ExeSpan{ExeSpan::Kind::Text, "after.loot.weapons", 0x46db, 0x10c6, 31},
    ExeSpan{ExeSpan::Kind::Text, "after.loot.swap_question", 0x46db, 0x10e5, 26},
    ExeSpan{ExeSpan::Kind::Text, "after.loot.swap_for", 0x46db, 0x10ff, 20},
    ExeSpan{ExeSpan::Kind::Text, "after.loot.swap_end", 0x46db, 0x1113, 2},
    // 15ab:0b5e:
    ExeSpan{ExeSpan::Kind::Text, "fight.computer_question", 0x46db, 0x1115, 43},
    // 15ab:1467, the Scan unit browser, and 15ab:18e8, the unit panel:
    ExeSpan{ExeSpan::Kind::Text, "scan.enemy", 0x46db, 0x1148, 7},
    ExeSpan{ExeSpan::Kind::Text, "scan.enemy_human", 0x46db, 0x114f, 12},
    ExeSpan{ExeSpan::Kind::Text, "scan.weapon", 0x46db, 0x115b, 11},
    ExeSpan{ExeSpan::Kind::Text, "scan.direction", 0x46db, 0x1166, 14},
    ExeSpan{ExeSpan::Kind::Text, "scan.menu", 0x46db, 0x1174, 21},
    ExeSpan{ExeSpan::Kind::Text, "scan.detail", 0x46db, 0x1189, 12},
    ExeSpan{ExeSpan::Kind::Text, "scan.done", 0x46db, 0x1195, 5},
    ExeSpan{ExeSpan::Kind::Text, "unit.possessive", 0x46db, 0x121e, 4},
    ExeSpan{ExeSpan::Kind::Text, "unit.suspicious", 0x46db, 0x1222, 35},
    ExeSpan{ExeSpan::Kind::Text, "unit.weapon_armour", 0x46db, 0x1245, 18},
    ExeSpan{ExeSpan::Kind::Text, "unit.ruined", 0x46db, 0x1257, 9},
    ExeSpan{ExeSpan::Kind::Text, "unit.none", 0x46db, 0x1260, 5},
    // 17dc:0629, the training missions:
    ExeSpan{ExeSpan::Kind::Text, "mission.failed", 0x46db, 0x1434, 55},
    ExeSpan{ExeSpan::Kind::Text, "mission.not_done", 0x46db, 0x146b, 54},
    ExeSpan{ExeSpan::Kind::Text, "mission.course_corner", 0x46db, 0x14a1, 78},
    ExeSpan{ExeSpan::Kind::Text, "mission.no_hands", 0x46db, 0x14ef, 59},
    ExeSpan{ExeSpan::Kind::Text, "mission.hands", 0x46db, 0x152a, 51},
    ExeSpan{ExeSpan::Kind::Text, "mission.board", 0x46db, 0x155d, 61},
    ExeSpan{ExeSpan::Kind::Text, "mission.board_2", 0x46db, 0x159a, 58},
    ExeSpan{ExeSpan::Kind::Text, "mission.switch", 0x46db, 0x15d4, 36},
    ExeSpan{ExeSpan::Kind::Text, "mission.starts", 0x46db, 0x15f8, 25},
    ExeSpan{ExeSpan::Kind::Text, "mission.wont_start", 0x46db, 0x1611, 33},
    // 1d43:0004, the Use Weapons screen, and 1d43:07cb, the target picker:
    ExeSpan{ExeSpan::Kind::Text, "weapons.shut_down", 0x46db, 0x2904, 41},
    ExeSpan{ExeSpan::Kind::Text, "weapons.sensors_damaged", 0x46db, 0x292d, 55},
    ExeSpan{ExeSpan::Kind::Text, "weapons.sensors_destroyed", 0x46db, 0x2964, 63},
    ExeSpan{ExeSpan::Kind::Text, "weapons.select", 0x46db, 0x29a3, 27},
    ExeSpan{ExeSpan::Kind::Text, "weapons.heading", 0x46db, 0x29be, 27},
    ExeSpan{ExeSpan::Kind::Text, "weapons.destroyed", 0x46db, 0x29d9, 10},
    ExeSpan{ExeSpan::Kind::Text, "weapons.ammo_full", 0x46db, 0x29e3, 5},
    ExeSpan{ExeSpan::Kind::Text, "weapons.ammo_out", 0x46db, 0x29e8, 4},
    ExeSpan{ExeSpan::Kind::Text, "weapons.space", 0x46db, 0x29ec, 2},
    ExeSpan{ExeSpan::Kind::Text, "weapons.human", 0x46db, 0x29ee, 6},
    ExeSpan{ExeSpan::Kind::Text, "weapons.none", 0x46db, 0x29f4, 5},
    ExeSpan{ExeSpan::Kind::Text, "weapons.done", 0x46db, 0x29f9, 7},
    ExeSpan{ExeSpan::Kind::Text, "weapons.choose_other", 0x46db, 0x2a00, 57},
    ExeSpan{ExeSpan::Kind::Text, "weapons.no_ammo", 0x46db, 0x2a39, 59},
    ExeSpan{ExeSpan::Kind::Text, "weapons.already_firing", 0x46db, 0x2a74, 52},
    ExeSpan{ExeSpan::Kind::Text, "weapons.choose_target", 0x46db, 0x2aa8, 30},
    ExeSpan{ExeSpan::Kind::Text, "weapons.using", 0x46db, 0x2ac6, 20},
    ExeSpan{ExeSpan::Kind::Text, "weapons.against", 0x46db, 0x2ada, 12},
    ExeSpan{ExeSpan::Kind::Text, "weapons.against_human", 0x46db, 0x2ae6, 6},
    ExeSpan{ExeSpan::Kind::Text, "weapons.at", 0x46db, 0x2aec, 5},
    ExeSpan{ExeSpan::Kind::Text, "weapons.range_override", 0x46db, 0x2af1, 27},
    ExeSpan{ExeSpan::Kind::Text, "pick.use", 0x46db, 0x2b0c, 10},
    ExeSpan{ExeSpan::Kind::Text, "pick.against", 0x46db, 0x2b16, 10},
    ExeSpan{ExeSpan::Kind::Text, "pick.target_range", 0x46db, 0x2b22, 20},
    ExeSpan{ExeSpan::Kind::Text, "pick.menu", 0x46db, 0x2b36, 30},
    ExeSpan{ExeSpan::Kind::Text, "pick.weapon", 0x46db, 0x2b54, 10},
    ExeSpan{ExeSpan::Kind::Text, "pick.human", 0x46db, 0x2b5e, 12},
    ExeSpan{ExeSpan::Kind::Text, "pick.blank", 0x46db, 0x2b6a, 3},
    ExeSpan{ExeSpan::Kind::Text, "destroyed.mech", 0x46db, 0x2b6d, 20},
    ExeSpan{ExeSpan::Kind::Text, "destroyed.eject", 0x46db, 0x2b81, 12},
    // 1e31: a critical hit (11ab), a person stepped on (19..), the names (1b33), a location (1b8f):
    ExeSpan{ExeSpan::Kind::Text, "shot.critical", 0x46db, 0x315e, 11},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_human", 0x46db, 0x319a, 12},
    ExeSpan{ExeSpan::Kind::Text, "shot.squashed", 0x46db, 0x31a6, 24},
    ExeSpan{ExeSpan::Kind::Text, "shot.location_end", 0x46db, 0x3240, 2},
    // 203b:000a, the fight: set-up, the encounter, the double agent, the end:
    ExeSpan{ExeSpan::Kind::Text, "fight.attackers", 0x46db, 0x3402, 18},
    ExeSpan{ExeSpan::Kind::Text, "fight.attackers_mech", 0x46db, 0x3414, 7},
    ExeSpan{ExeSpan::Kind::Text, "fight.attackers_and", 0x46db, 0x341b, 6},
    ExeSpan{ExeSpan::Kind::Text, "fight.attackers_human", 0x46db, 0x3421, 7},
    ExeSpan{ExeSpan::Kind::Text, "fight.engage", 0x46db, 0x3428, 18},
    ExeSpan{ExeSpan::Kind::Text, "fight.not_evaded", 0x46db, 0x343a, 24},
    ExeSpan{ExeSpan::Kind::Text, "fight.raid_mechs", 0x46db, 0x3452, 69},
    ExeSpan{ExeSpan::Kind::Text, "fight.no_escape_jenners", 0x46db, 0x3497, 33},
    ExeSpan{ExeSpan::Kind::Text, "fight.no_escape", 0x46db, 0x34b8, 49},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.0", 0x46db, 0x34e9, 13},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.1", 0x46db, 0x34f6, 26},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.2", 0x46db, 0x3510, 69},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.3", 0x46db, 0x3555, 57},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.4", 0x46db, 0x358e, 67},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.5", 0x46db, 0x35d1, 63},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.6", 0x46db, 0x3610, 67},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.7", 0x46db, 0x3653, 19},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.8", 0x46db, 0x3666, 67},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.9", 0x46db, 0x36a9, 77},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.10", 0x46db, 0x36f6, 59},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.11", 0x46db, 0x3731, 67},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.12", 0x46db, 0x3774, 68},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.13", 0x46db, 0x37b8, 69},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.14", 0x46db, 0x37fd, 51},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.15", 0x46db, 0x3830, 74},
    ExeSpan{ExeSpan::Kind::Text, "fight.agent.16", 0x46db, 0x387a, 66},
    ExeSpan{ExeSpan::Kind::Text, "fight.some_fled", 0x46db, 0x38bc, 36},
    ExeSpan{ExeSpan::Kind::Text, "fight.all_fled", 0x46db, 0x38e0, 28},
    ExeSpan{ExeSpan::Kind::Text, "fight.beat_people", 0x46db, 0x38fc, 58},
    ExeSpan{ExeSpan::Kind::Text, "fight.eluded", 0x46db, 0x3936, 30},
    ExeSpan{ExeSpan::Kind::Text, "fight.return", 0x46db, 0x3954, 55},
    ExeSpan{ExeSpan::Kind::Text, "fight.return_to_base", 0x46db, 0x398b, 31},
    ExeSpan{ExeSpan::Kind::Text, "fight.arena_won", 0x46db, 0x39aa, 32},
    ExeSpan{ExeSpan::Kind::Text, "fight.arena_lost", 0x46db, 0x39ca, 45},
    ExeSpan{ExeSpan::Kind::Text, "fight.eluded_2", 0x46db, 0x39f7, 30},
    // 203b:0fa9, by the encounter (pointers at DG:3a2e):
    ExeSpan{ExeSpan::Kind::Text, "fight.encounter.0", 0x46db, 0x32e6, 67},
    ExeSpan{ExeSpan::Kind::Text, "fight.encounter.1", 0x46db, 0x3329, 65},
    ExeSpan{ExeSpan::Kind::Text, "fight.encounter.2", 0x46db, 0x336a, 56},
    ExeSpan{ExeSpan::Kind::Text, "fight.encounter.3", 0x46db, 0x33a2, 60},
    ExeSpan{ExeSpan::Kind::Text, "fight.encounter.4", 0x46db, 0x33de, 36},
    // 203b:14c3, the orders menu, and its helpers (1c1f, 1e46, 2223, 24d2, 2591):
    ExeSpan{ExeSpan::Kind::Text, "orders.computer_moving", 0x46db, 0x3a42, 45},
    ExeSpan{ExeSpan::Kind::Text, "orders.menu_mech", 0x46db, 0x3a6f, 77},
    ExeSpan{ExeSpan::Kind::Text, "orders.menu_person", 0x46db, 0x3abc, 74},
    ExeSpan{ExeSpan::Kind::Text, "orders.now", 0x46db, 0x3b24, 18},
    ExeSpan{ExeSpan::Kind::Text, "orders.now_n", 0x46db, 0x3b36, 2},
    ExeSpan{ExeSpan::Kind::Text, "orders.now_ing", 0x46db, 0x3b38, 5},
    ExeSpan{ExeSpan::Kind::Text, "orders.wish", 0x46db, 0x3b3d, 17},
    ExeSpan{ExeSpan::Kind::Text, "orders.instead", 0x46db, 0x3b4e, 10},
    ExeSpan{ExeSpan::Kind::Text, "orders.deleted", 0x46db, 0x3b58, 52},
    ExeSpan{ExeSpan::Kind::Text, "orders.new_moves", 0x46db, 0x3b8c, 33},
    ExeSpan{ExeSpan::Kind::Text, "orders.no_jump_jets", 0x46db, 0x3bad, 29},
    ExeSpan{ExeSpan::Kind::Text, "orders.overheated", 0x46db, 0x3bca, 41},
    ExeSpan{ExeSpan::Kind::Text, "orders.inhibited", 0x46db, 0x3bf3, 26},
    ExeSpan{ExeSpan::Kind::Text, "orders.inhibited_legs", 0x46db, 0x3c0d, 13},
    ExeSpan{ExeSpan::Kind::Text, "orders.inhibited_heat", 0x46db, 0x3c1a, 25},
    ExeSpan{ExeSpan::Kind::Text, "orders.cant", 0x46db, 0x3c33, 11},
    ExeSpan{ExeSpan::Kind::Text, "orders.move", 0x46db, 0x3c3e, 5},
    ExeSpan{ExeSpan::Kind::Text, "orders.no_further", 0x46db, 0x3c43, 24},
    ExeSpan{ExeSpan::Kind::Text, "orders.try_running", 0x46db, 0x3c5b, 46},
    ExeSpan{ExeSpan::Kind::Text, "orders.you_have", 0x46db, 0x3c89, 10},
    ExeSpan{ExeSpan::Kind::Text, "orders.points_left", 0x46db, 0x3c93, 65},
    ExeSpan{ExeSpan::Kind::Text, "orders.choose_kick", 0x46db, 0x3ce0, 26},
    ExeSpan{ExeSpan::Kind::Text, "orders.cant_kick", 0x46db, 0x3cfa, 64},
    ExeSpan{ExeSpan::Kind::Text, "options.messages", 0x46db, 0x3d3a, 36},
    ExeSpan{ExeSpan::Kind::Text, "options.graphics", 0x46db, 0x3d5e, 21},
    ExeSpan{ExeSpan::Kind::Text, "scan.which", 0x46db, 0x3d73, 33},
    ExeSpan{ExeSpan::Kind::Text, "scan.humans_only", 0x46db, 0x3d94, 57},
    ExeSpan{ExeSpan::Kind::Text, "scan.no_sensors", 0x46db, 0x3dcd, 76},
    ExeSpan{ExeSpan::Kind::Text, "scan.no_humans", 0x46db, 0x3e19, 42},
    // 203b:1cfe, the movement modes (pointers at DG:3cd4):
    ExeSpan{ExeSpan::Kind::Text, "orders.mode.0", 0x46db, 0x3b16, 5},
    ExeSpan{ExeSpan::Kind::Text, "orders.mode.1", 0x46db, 0x3b1b, 4},
    ExeSpan{ExeSpan::Kind::Text, "orders.mode.2", 0x46db, 0x3b1f, 5},
    // 22e8:000c, one round: the shot's message pieces (docs/combat.md):
    ExeSpan{ExeSpan::Kind::Text, "shot.party_mech", 0x46db, 0x3e44, 9},
    ExeSpan{ExeSpan::Kind::Text, "shot.party_kicks", 0x46db, 0x3e4d, 6},
    ExeSpan{ExeSpan::Kind::Text, "shot.party_kick", 0x46db, 0x3e53, 5},
    ExeSpan{ExeSpan::Kind::Text, "shot.party_uses", 0x46db, 0x3e58, 8},
    ExeSpan{ExeSpan::Kind::Text, "shot.party_person_uses", 0x46db, 0x3e60, 9},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy", 0x46db, 0x3e69, 10},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_human_uses", 0x46db, 0x3e73, 14},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_mech", 0x46db, 0x3e81, 6},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_uses", 0x46db, 0x3e87, 8},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_kicks", 0x46db, 0x3e8f, 6},
    ExeSpan{ExeSpan::Kind::Text, "shot.enemy_kick", 0x46db, 0x3e95, 5},
    ExeSpan{ExeSpan::Kind::Text, "shot.on", 0x46db, 0x3e9a, 5},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_mech", 0x46db, 0x3e9f, 8},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_end", 0x46db, 0x3ea7, 2},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_enemy", 0x46db, 0x3ea9, 10},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_enemy_mech", 0x46db, 0x3eb3, 6},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_enemy_human", 0x46db, 0x3eb9, 7},
    ExeSpan{ExeSpan::Kind::Text, "shot.target_spectator", 0x46db, 0x3ec0, 13},
    ExeSpan{ExeSpan::Kind::Text, "shot.person_missed", 0x46db, 0x3ecd, 9},
    ExeSpan{ExeSpan::Kind::Text, "shot.person_hit", 0x46db, 0x3ed6, 8},
    ExeSpan{ExeSpan::Kind::Text, "shot.mech_missed", 0x46db, 0x3ede, 9},
    ExeSpan{ExeSpan::Kind::Text, "shot.mech_hit", 0x46db, 0x3ee7, 8},
    // 22e8:12c7, after the shot:
    ExeSpan{ExeSpan::Kind::Text, "shot.killed", 0x46db, 0x41fc, 13},
    ExeSpan{ExeSpan::Kind::Text, "shot.arena_damaged", 0x46db, 0x4209, 44},
    ExeSpan{ExeSpan::Kind::Text, "shot.urbanmechs", 0x46db, 0x4235, 57},
    // 1e31:031e, a person's condition by hit points (pointers at DG:2e0c):
    ExeSpan{ExeSpan::Kind::Text, "condition.0", 0x46db, 0x2bb4, 27},
    ExeSpan{ExeSpan::Kind::Text, "condition.1", 0x46db, 0x2bcf, 31},
    ExeSpan{ExeSpan::Kind::Text, "condition.2", 0x46db, 0x2bee, 25},
    ExeSpan{ExeSpan::Kind::Text, "condition.3", 0x46db, 0x2c07, 26},
    ExeSpan{ExeSpan::Kind::Text, "condition.4", 0x46db, 0x2c21, 30},
    ExeSpan{ExeSpan::Kind::Text, "condition.5", 0x46db, 0x2c3f, 23},
    ExeSpan{ExeSpan::Kind::Text, "condition.6", 0x46db, 0x2c56, 32},
    ExeSpan{ExeSpan::Kind::Text, "condition.7", 0x46db, 0x2c76, 26},
    ExeSpan{ExeSpan::Kind::Text, "condition.8", 0x46db, 0x2c90, 25},
    ExeSpan{ExeSpan::Kind::Text, "condition.9", 0x46db, 0x2ca9, 28},
    ExeSpan{ExeSpan::Kind::Text, "condition.10", 0x46db, 0x2cc5, 27},
    // 1d43, the range bands (pointers at DG:2ebc):
    ExeSpan{ExeSpan::Kind::Text, "weapons.range.0", 0x46db, 0x2ce0, 6},
    ExeSpan{ExeSpan::Kind::Text, "weapons.range.1", 0x46db, 0x2ce6, 4},
    ExeSpan{ExeSpan::Kind::Text, "weapons.range.2", 0x46db, 0x2cea, 5},
    ExeSpan{ExeSpan::Kind::Text, "weapons.range.3", 0x46db, 0x2cef, 4},
    // 1e31:1b8f, the locations, by location index (armour byte 0x11 + index; the names' order and DG:3242 differ):
    ExeSpan{ExeSpan::Kind::Text, "location.0", 0x46db, 0x31d2, 9},
    ExeSpan{ExeSpan::Kind::Text, "location.1", 0x46db, 0x31be, 11},
    ExeSpan{ExeSpan::Kind::Text, "location.2", 0x46db, 0x31c9, 9},
    ExeSpan{ExeSpan::Kind::Text, "location.3", 0x46db, 0x3229, 5},
    ExeSpan{ExeSpan::Kind::Text, "location.4", 0x46db, 0x321c, 13},
    ExeSpan{ExeSpan::Kind::Text, "location.5", 0x46db, 0x3201, 10},
    ExeSpan{ExeSpan::Kind::Text, "location.6", 0x46db, 0x31eb, 12},
    ExeSpan{ExeSpan::Kind::Text, "location.7", 0x46db, 0x31f7, 10},
    ExeSpan{ExeSpan::Kind::Text, "location.8", 0x46db, 0x31db, 16},
    ExeSpan{ExeSpan::Kind::Text, "location.9", 0x46db, 0x322e, 18},
    ExeSpan{ExeSpan::Kind::Text, "location.10", 0x46db, 0x320b, 17},
    // The personal armour types (pointers at DG:4e8a):
    ExeSpan{ExeSpan::Kind::Text, "armour.0", 0x46db, 0x46a2, 5},
    ExeSpan{ExeSpan::Kind::Text, "armour.1", 0x46db, 0x46a7, 10},
    ExeSpan{ExeSpan::Kind::Text, "armour.2", 0x46db, 0x46b1, 10},
    ExeSpan{ExeSpan::Kind::Text, "armour.3", 0x46db, 0x46bb, 12},
    ExeSpan{ExeSpan::Kind::Text, "armour.4", 0x46db, 0x46c7, 12},
    ExeSpan{ExeSpan::Kind::Text, "armour.5", 0x46db, 0x46d3, 9},
};

constexpr std::uint16_t kLoadSegment = 0x1000;

}  // namespace

std::span<const ExeSpan> OriginalExe::table() { return kDos103; }

std::string OriginalExe::address(const ExeSpan& span) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04x:%04x", span.segment, span.offset);
    return buf;
}

OriginalExe::OriginalExe(std::span<const std::uint8_t> packed) {
    const std::string wanted =
        "this BTECH.EXE is not a version the rebuild knows (it supports " +
        std::string(kSupportedVersion) + ")";
    try {
        unpacked_ = unexepack(packed);
    } catch (const FormatError& e) {
        throw FormatError(wanted + ": " + e.what());
    }
    if (sha256_hex(unpacked_) != kSupportedUnpackedSha256) throw FormatError(wanted);
    image_start_ = std::size_t(unpacked_[8] | unpacked_[9] << 8) * 16;
}

OriginalExe OriginalExe::load(const std::filesystem::path& game_dir) {
    const auto path = find_game_file(game_dir, "BTECH.EXE");
    if (!path) throw FormatError("BTECH.EXE not found in " + game_dir.string());
    return OriginalExe(read_file(*path));
}

std::span<const std::uint8_t> OriginalExe::get(std::string_view name) const {
    const auto it = std::find_if(kDos103.begin(), kDos103.end(),
                                 [&](const ExeSpan& s) { return s.name == name; });
    if (it == kDos103.end()) throw FormatError("no piece of BTECH.EXE called " + std::string(name));
    const std::size_t at = image_start_ + std::size_t(it->segment - kLoadSegment) * 16 + it->offset;
    if (at + it->size > unpacked_.size()) throw FormatError("BTECH.EXE: piece out of range");
    return std::span(unpacked_).subspan(at, it->size);
}

std::string OriginalExe::text(std::string_view name) const { return text_of(get(name)); }

std::string OriginalExe::text_of(std::span<const std::uint8_t> bytes) {
    std::size_t n = 0;
    while (n < bytes.size()) {
        const std::uint8_t c = bytes[n];
        if (c == 0) break;
        ++n;
        if (c & 0x80) break;
        if (c == 0x02 || c == 0x06 || c == 0x09 || c == 0x13) ++n;  // and its argument
    }
    return std::string(bytes.begin(), bytes.begin() + std::ptrdiff_t(std::min(n, bytes.size())));
}

}  // namespace chr::formats
