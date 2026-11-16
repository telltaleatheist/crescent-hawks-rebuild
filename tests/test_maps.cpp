#include <vector>

#include "formats/cmp.h"
#include "formats/game_files.h"
#include "formats/icn.h"
#include "formats/mtp.h"
#include "formats/tables.h"
#include "game/map_view.h"
#include "test.h"

using namespace chr;
using namespace chr::formats;

namespace {

// A 16x8 map (two blocks side by side) with every table empty: block 0's
// cells are 0..63 and block 1's are 100..163.
std::vector<std::uint8_t> make_mtp() {
    std::vector<std::uint8_t> file(0x21d, 0);
    file[3] = 16;
    file[4] = 8;
    const char* person = "Someone";
    for (int i = 0; person[i]; ++i) file[5 + i] = std::uint8_t(person[i]);
    file[0x185 + 2] = 0x45;  // entrances[1].x = 0x0c45
    file[0x185 + 3] = 0x0c;
    for (int b = 0; b < 2; ++b)
        for (int i = 0; i < 64; ++i) file.push_back(std::uint8_t(b * 100 + i));
    return file;
}

}  // namespace

TEST(mtp_tables_and_block_order) {
    const auto map = decode_mtp(make_mtp());
    CHECK(map.width == 16 && map.height == 8);
    CHECK(map.people[0] == "Someone");
    CHECK(map.entrances[1].x == 0x0c45);
    CHECK(map.cell(0, 0) == 0);
    CHECK(map.cell(7, 0) == 7);
    CHECK(map.cell(0, 1) == 8);     // the next row of block 0, not cell 16 of the file
    CHECK(map.cell(8, 0) == 100);   // block 1
    CHECK(map.cell(15, 7) == 163);
}

TEST(mtp_wrong_size_is_an_error) {
    auto file = make_mtp();
    file.pop_back();
    bool threw = false;
    try {
        decode_mtp(file);
    } catch (const FormatError&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(view_origin_and_extent) {
    const auto map = decode_mtp(make_mtp());
    const Tables tables;
    const game::Fixes fixes;
    game::Terrain terrain(tables, fixes);
    terrain.place_map(map, 4);
    terrain.link_blocks();
    // Half-cell (13, 12): the view starts at half-cell (1, 0), i.e. half-way
    // into cell 0, and is 27 half-cells across and 25 down: cells 0..13 and
    // 0..12, those past the map's edge from the terrain.
    const auto view = game::view_of(terrain, "BTTLTECH.ICN", MapPosition{13, 12});
    CHECK(view.origin_half_x == 1 && view.origin_half_y == 0);
    CHECK(view.cells.size() == std::size_t(14 * 13));
    CHECK(view.cells.front().tile == (AssetId{"BTTLTECH.ICN", map.cell(0, 0)}));
    CHECK(view.cells.back().cell_x == 13 && view.cells.back().cell_y == 12);
}

TEST(new_game_starts_by_the_citadel) {
    const auto start = game::new_game_position();
    CHECK(start.half_cell_x() == 69 && start.half_cell_y() == 25);
    CHECK(game::tile_sheet_for_map(1) == "BTTLTECH.ICN");
    CHECK(game::tile_sheet_for_map(11) == "DESTRUCT.ICN");
}

TEST(shipped_maps_and_sheets_decode) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) {
        chrtest::skip("no game folder (set CHR_GAME_DIR)");
        return;
    }
    for (int n = 1; n <= 14; ++n) {
        const auto path = find_game_file(dir, "MAP" + std::to_string(n) + ".MTP");
        CHECK(path.has_value());
        if (!path) continue;
        const auto map = decode_mtp(read_file(*path));
        CHECK(map.block_x == 0 && map.block_y == 0);
        for (auto b : map.blocks) CHECK(b < kTilesPerSheet);
    }
    const auto map1 = decode_mtp(read_file(*find_game_file(dir, "MAP1.MTP")));
    CHECK(map1.places[1] == "Citadel");
    CHECK(map1.entrances[1].x == game::new_game_position().x);
    for (const char* name : {"BTTLTECH.ICN", "DESTRUCT.ICN", "STARLEAG.ICN", "MAP.ICN", "ANIMATE.ICN"}) {
        const auto path = find_game_file(dir, name);
        CHECK(path.has_value());
        if (path) CHECK(decode_icn(read_file(*path)).packed.size() == 32000);
    }
}
