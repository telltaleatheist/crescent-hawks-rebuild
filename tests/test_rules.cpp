// The rule tables: imported from the player's BTECH.EXE, written as JSON and
// read back, they must give the program's bytes exactly. No values here:
// the repo never holds them (docs/rules.md).
#include <filesystem>

#include "formats/exe_data.h"
#include "formats/game_files.h"
#include "formats/import.h"
#include "formats/rules.h"
#include "test.h"

using namespace chr;

TEST(rules_round_trip_to_the_programs_bytes) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) {
        chrtest::skip("no game folder (set CHR_GAME_DIR)");
        return;
    }
    const auto exe = formats::OriginalExe::load(dir);
    const auto rules = formats::rules_from_original(exe);
    const auto back = formats::read_rules_files(formats::write_rules_files(rules, formats::kImportVersion));

    const auto mechs = exe.get("rules.mech_templates");
    CHECK(back.mechs.size() == 8);
    for (std::size_t i = 0; i < back.mechs.size(); ++i) {
        CHECK(back.mechs[i].id == formats::mech_template_ids()[i]);
        CHECK(std::equal(back.mechs[i].record.begin(), back.mechs[i].record.end(),
                         mechs.begin() + std::ptrdiff_t(i * formats::kMechRecordSize)));
    }
    const auto weapons = exe.get("rules.weapons");
    CHECK(back.weapons.size() == 0x21);
    for (std::size_t i = 0; i < back.weapons.size(); ++i)
        for (int b = formats::kWeaponNameSize; b < formats::kWeaponRecordSize; ++b)
            CHECK(back.weapons[i][std::size_t(b)] == weapons[i * formats::kWeaponRecordSize + std::size_t(b)]);

    // The combat tables, against the bytes as the program indexes them.
    const auto& c = back.combat;
    const auto hit = exe.get("rules.hit_locations");
    for (int s = 0; s < 4; ++s)
        for (int r = 0; r < 11; ++r) {
            CHECK(c.hit_locations[s][r] + 0x11 == hit[std::size_t(s * 11 + r)]);
            CHECK(c.hit_locations[s][r] <= 10);
        }
    const auto clusters = exe.get("rules.clusters");
    for (int r = 0; r < 11; ++r)
        for (int col = 0; col < 7; ++col) CHECK(c.clusters[r][col] == clusters[std::size_t(r * 7 + col)]);
    const auto same = [&](const auto& got, const char* name) {
        const auto b = exe.get(name);
        CHECK(std::equal(got.begin(), got.end(), b.begin()));
    };
    same(c.facing_sides, "rules.facing_sides");
    same(c.target_movement, "rules.target_movement");
    same(c.difficulty_armour, "rules.difficulty_armour");
    same(c.personal_armour, "rules.personal_armour");
}

TEST(import_writes_the_rules_and_names) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) {
        chrtest::skip("no game folder (set CHR_GAME_DIR)");
        return;
    }
    const auto data = std::filesystem::path(CHR_TEST_SCRATCH) / "import-rules";
    std::filesystem::remove_all(data);
    formats::import_original(dir, data);
    const auto rules = formats::read_rules(data);
    CHECK(rules.mechs.size() == 8 && rules.weapons.size() == 0x21);
    const auto bytes = formats::read_file(formats::imported_text_path(data));
    const std::string json(bytes.begin(), bytes.end());
    for (const auto id : formats::mech_template_ids())
        CHECK(json.find("\"id\": \"" + formats::mech_text_id(id) + "\"") != std::string::npos);
    CHECK(json.find("\"id\": \"weapon.32\", \"address\": \"46db:30f8\"") != std::string::npos);
}
