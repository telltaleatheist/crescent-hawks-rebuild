#include <memory>
#include <string>
#include <vector>

#include "formats/exe_data.h"
#include "formats/game_files.h"
#include "formats/mtp.h"
#include "game/map_view.h"
#include "game/movement.h"
#include "game/town.h"
#include "test.h"

using namespace chr;
using namespace chr::formats;
using namespace chr::game;

namespace {

// A 16x16 map of open ground (tile 0) with a wall (tile 0x55) along cell
// row 3, and entrance 0 at half-cell (10, 20).
MtpMap open_map() {
    MtpMap map;
    map.width = 16;
    map.height = 16;
    map.blocks.assign(256, 0);
    for (int x = 0; x < 8; ++x) {
        map.blocks[3 * 8 + x] = 0x55;        // block 0, row 3
        map.blocks[64 + 3 * 8 + x] = 0x55;   // block 1, row 3
    }
    map.places[0] = "Somewhere";
    map.entrances[0] = {10, 20};
    for (int i = 1; i < 16; ++i) map.entrances[i] = {0xffff, 0xffff};
    return map;
}

struct Answer : EntranceQuestion {
    bool yes = false;
    int asked = -1;
    bool ask(int place) override {
        asked = place;
        return yes;
    }
};

}  // namespace

TEST(keys_give_directions) {
    CHECK(step_for_key(key::kUp)->dy == -1 && step_for_key(key::kUp)->dx == 0);
    CHECK(step_for_key(key::kPageDown)->dx == 1 && step_for_key(key::kPageDown)->dy == 1);
    CHECK(step_for_key(key::kHome)->dx == -1 && step_for_key(key::kHome)->dy == -1);
    CHECK(!step_for_key('x'));
}

TEST(step_moves_a_half_cell) {
    const auto map = open_map();
    Answer no;
    const auto r = take_step(map, 1, {10, 12}, Party{}, {1, 1}, no);
    CHECK(r.outcome == StepOutcome::Moved);
    CHECK(r.position == (MapPosition{11, 13}));
}

TEST(wall_blocks) {
    const auto map = open_map();
    Answer no;
    // Half-cell row 8 is cell row 4; one up is half-cell 7, cell row 3: the wall.
    const auto r = take_step(map, 1, {10, 8}, Party{}, {0, -1}, no);
    CHECK(r.outcome == StepOutcome::Blocked);
    CHECK(r.position == (MapPosition{10, 8}));
}

TEST(entrance_asks_and_then_checks_the_step) {
    const auto map = open_map();
    Answer yes;
    yes.yes = true;
    const auto r = take_step(map, 1, {10, 21}, Party{}, {0, -1}, yes);
    CHECK(yes.asked == 0 && r.entered);
    CHECK(r.outcome == StepOutcome::Moved);
}

TEST(map_edge_is_leaving) {
    const auto map = open_map();
    Answer no;
    CHECK(take_step(map, 1, {10, 0}, Party{}, {0, -1}, no).outcome == StepOutcome::LeftMap);
    CHECK(take_step(map, 1, {31, 20}, Party{}, {1, 0}, no).outcome == StepOutcome::LeftMap);
}

TEST(key_script_parses) {
    const auto keys = parse_key_script("up*3, pgdn y");
    CHECK(keys.size() == 5);
    CHECK(keys[0] == key::kUp && keys[2] == key::kUp && keys[3] == key::kPageDown && keys[4] == 'y');
    bool threw = false;
    try {
        parse_key_script("jump");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

namespace {

// Plays `script` on map 1 from the new-game start. False if the game files
// are not here (the test is skipped).
bool play_map1(const std::string& script, std::unique_ptr<Town>& town, MtpMap& map,
               std::unique_ptr<OriginalExe>& exe) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) {
        chrtest::skip("no game folder (set CHR_GAME_DIR)");
        return false;
    }
    map = decode_mtp(read_file(*find_game_file(dir, "MAP1.MTP")));
    exe = std::make_unique<OriginalExe>(OriginalExe::load(dir));
    town = std::make_unique<Town>(map, 1, new_game_position(), *exe);
    town->draw_panel();
    ScriptKeys keys(parse_key_script(script));
    town->run(keys);
    return true;
}

}  // namespace

TEST(map1_wall_south_of_the_start) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("down*4", town, map, exe)) return;
    // Two steps south, then the fence.
    CHECK(town->position() == (MapPosition{0x0c45, 0xc01b}));
    CHECK(town->log()[1] == "moved at 0c45,c01b");
    CHECK(town->log()[2] == "blocked at 0c45,c01b");
}

TEST(map1_citadel_question) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("up*5", town, map, exe)) return;
    CHECK(town->question_open());
    CHECK(town->position() == (MapPosition{0x0c45, 0xc015}));
    const auto& t = town->text();
    CHECK(t.row_text(10).substr(14, 18) == "Will you enter the");
    CHECK(t.row_text(11).substr(14, 8) == "Citadel?");
    CHECK(t.row_text(12).substr(14, 6) == "Yes No");
    CHECK(t.cell(14, 12).bg == 14 && t.cell(14, 12).fg == 0);  // Yes picked: black on yellow
    CHECK(t.cell(18, 12).bg == 0 && t.cell(18, 12).fg == 15);
}

TEST(map1_enter_the_citadel) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("up*5 right y", town, map, exe)) return;
    CHECK(town->entered_place() == 1);
    // The right arrow picked No, then y answered yes: the line shows Yes picked.
    CHECK(town->text().cell(14, 12).bg == 14);
    CHECK(town->text().cell(18, 12).bg == 0);
}

TEST(map1_say_no_and_walk_on) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("up*5 n down", town, map, exe)) return;
    CHECK(!town->entered_place());
    CHECK(town->position() == (MapPosition{0x0c45, 0xc015}));
    // The next turn draws the map over the question.
    CHECK(town->text().cell(14, 10).kind == TextCell::Kind::Empty);
}

TEST(map1_view_scrolls_with_the_party) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("left*10", town, map, exe)) return;
    CHECK(town->position() == (MapPosition{0x0c3b, 0xc019}));
    const auto view = town->view();
    // The view's corner is 12 half-cells up and left of the party.
    CHECK(view.origin_half_x == 0x3b - 12 && view.origin_half_y == 0x19 - 12);
}

TEST(map1_panel_lists_jason) {
    std::unique_ptr<Town> town;
    MtpMap map;
    std::unique_ptr<OriginalExe> exe;
    if (!play_map1("", town, map, exe)) return;
    CHECK(town->text().row_text(13).substr(9, 3) == "BDC");
    CHECK(town->text().row_text(14).substr(1, 5) == "Jason");
    CHECK(town->text().cell(0, 0).kind == TextCell::Kind::Frame);
}
