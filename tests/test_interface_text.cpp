#include <array>
#include <string_view>

#include "formats/import.h"
#include "game/text_provider.h"
#include "test.h"

using namespace chr;

namespace {

// Every ID the repo's default interface file may hold (docs/text.md,
// "Interface text and imported text"). An ID is added here, by hand, only
// when it is a menu, prompt, label, message piece or rule message; when in
// doubt it stays imported only.
constexpr std::array<std::string_view, 151> kInterfaceIds = {
    "walk.too_deep", "walk.enter_question", "walk.enter_question_end", "walk.mech_enter_question",
    "walk.mech_enter_question_end",
    "walk.direction.north", "walk.direction.northeast", "walk.direction.east",
    "walk.direction.southeast", "walk.direction.south", "walk.direction.southwest",
    "walk.direction.west", "walk.direction.northwest", "ui.answer_yes_picked",
    "ui.answer_no_picked", "panel.party_heading", "panel.cbills_label",
    "intro.first_time", "intro.load_old_game", "talk.nobody",
    "talk.you_see", "talk.done", "talk.end",
    "talk.line_end", "after.repair.break", "after.scrap.plural",
    "after.scrap.end", "after.salvage.question", "after.loot.swap_question",
    "after.loot.swap_for", "after.loot.swap_end", "fight.computer_question",
    "scan.enemy", "scan.enemy_human", "scan.weapon",
    "scan.direction", "scan.menu", "scan.detail",
    "scan.done", "unit.possessive", "unit.weapon_armour",
    "unit.ruined", "unit.none", "weapons.shut_down",
    "weapons.sensors_damaged", "weapons.sensors_destroyed", "weapons.select",
    "weapons.heading", "weapons.destroyed", "weapons.ammo_full",
    "weapons.ammo_out", "weapons.space", "weapons.human",
    "weapons.none", "weapons.done", "weapons.choose_other",
    "weapons.no_ammo", "weapons.already_firing", "weapons.choose_target",
    "weapons.using", "weapons.against", "weapons.against_human",
    "weapons.at", "weapons.range_override", "pick.use",
    "pick.against", "pick.target_range", "pick.menu",
    "pick.weapon", "pick.human", "pick.blank",
    "destroyed.mech", "destroyed.eject", "shot.critical",
    "shot.enemy_human", "shot.location_end", "fight.attackers",
    "fight.attackers_mech", "fight.attackers_and", "fight.attackers_human",
    "fight.engage", "orders.computer_moving", "orders.menu_mech",
    "orders.menu_person", "orders.now", "orders.now_n",
    "orders.now_ing", "orders.wish", "orders.instead",
    "orders.deleted", "orders.new_moves", "orders.no_jump_jets",
    "orders.overheated", "orders.inhibited", "orders.inhibited_legs",
    "orders.inhibited_heat", "orders.cant", "orders.move",
    "orders.no_further", "orders.try_running", "orders.you_have",
    "orders.points_left", "orders.choose_kick", "orders.cant_kick",
    "options.messages", "options.graphics", "scan.which",
    "scan.humans_only", "scan.no_sensors", "scan.no_humans",
    "orders.mode.0", "orders.mode.1", "orders.mode.2",
    "shot.party_mech", "shot.party_kicks", "shot.party_kick",
    "shot.party_uses", "shot.party_person_uses", "shot.enemy",
    "shot.enemy_human_uses", "shot.enemy_mech", "shot.enemy_uses",
    "shot.enemy_kicks", "shot.enemy_kick", "shot.on",
    "shot.target_mech", "shot.target_end", "shot.target_enemy",
    "shot.target_enemy_mech", "shot.target_enemy_human", "shot.target_spectator",
    "shot.person_missed", "shot.person_hit", "shot.mech_missed",
    "shot.mech_hit", "shot.killed", "weapons.range.0",
    "weapons.range.1", "weapons.range.2", "weapons.range.3",
    "location.0", "location.1", "location.2",
    "location.3", "location.4", "location.5",
    "location.6", "location.7", "location.8",
    "location.9", "location.10",
};

game::TextTable interface_file() { return game::TextTable::load(CHR_INTERFACE_TEXT); }

}  // namespace

// The default file holds the listed IDs and nothing else.
TEST(interface_text_holds_only_interface_ids) {
    const auto table = interface_file();
    CHECK(table.size() == kInterfaceIds.size());
    for (const auto id : kInterfaceIds)
        if (!table.find(id)) chrtest::fail(__FILE__, __LINE__, std::string(id) + " is missing from interface.json");
}

// Each default text is the one the importer reads from the player's program.
TEST(interface_text_matches_the_import) {
    const auto data = chrtest::imported_data_dir();
    if (data.empty()) return chrtest::skip("no game folder (set CHR_GAME_DIR)");
    const auto imported = game::TextTable::load(formats::imported_text_path(data));
    const auto table = interface_file();
    for (const auto id : kInterfaceIds)
        if (table.find(id) != imported.find(id))
            chrtest::fail(__FILE__, __LINE__, std::string(id) + " differs from the import");
}
