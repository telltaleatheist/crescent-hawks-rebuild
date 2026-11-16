#include <set>

#include "formats/bld.h"
#include "formats/game_files.h"
#include "formats/menus.h"
#include "test.h"

using namespace chr;
using namespace chr::formats;

TEST(bld_cipher_and_length_word) {
    CHECK(decipher_bld_byte(0x00) == 0xC0);
    CHECK(decipher_bld_byte(0xD7) == 0xE9);
    // A length word, then the enciphered bytes; anything after is ignored.
    const std::vector<std::uint8_t> file{0x02, 0x00, 0xFE, 0xD7, 0x55};
    const auto script = parse_bld(file);
    CHECK(script.size() == 2 && script[0] == decipher_bld_byte(0xFE) && script[1] == 0xE9);
}

TEST(bld_decode_operands) {
    // window 6; print "Hi"; if_flag 3 -> 0x0010; menu 6 (2 items); end
    const std::vector<std::uint8_t> s{0xFE, 6, 0xFC, 'H', 'i', 0, 0xF7, 3, 0x10, 0x00,
                                      0xF9, 6, 0x20, 0x00, 0x30, 0x00, 0x05, 0xFF};
    auto in = decode(s, 0);
    CHECK(in.op == Op::Window && in.bytes == std::vector<int>{6} && in.next == 2);
    in = decode(s, 2);
    CHECK(in.op == Op::Print && in.text == "Hi" && in.next == 6);
    in = decode(s, 6);
    CHECK(in.op == Op::IfFlag && in.bytes[0] == 3 && in.targets == std::vector<int>{0x10} && in.falls_through);
    in = decode(s, 10);
    CHECK(in.op == Op::Menu && in.targets == (std::vector<int>{0x20, 0x30}) && !in.falls_through);
    CHECK(kMenus[6].items == 2);
    in = decode(s, 16);
    CHECK(!in.op && in.next == 17);  // not an opcode: stepped over
    CHECK(bld_text_id("CITADEL", 0xf3) == "bld.citadel.00f3");
}

TEST(bld_walk_reaches_every_text) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) return chrtest::skip("no game files");
    for (const auto file : kBldFiles) {
        const auto path = find_game_file(dir, std::string(file) + ".BLD");
        CHECK(path.has_value());
        if (!path) continue;
        const auto script = read_bld(*path);
        const auto code = walk(script);
        std::set<int> covered;
        int texts = 0;
        for (const auto& [at, in] : code) {
            for (int i = at; i < in.next; ++i) covered.insert(i);
            if (in.op == Op::Print) ++texts;
        }
        // Every byte is reached but the two zero bytes at the end, save
        // WEAPON's dead block (docs/scripts.md) and two stray bytes after
        // an `end` in TRAINING.
        const int unreached = static_cast<int>(script.size() - covered.size());
        int expected = 2;
        if (file == "WEAPON") expected = 2 + 192;
        if (file == "TRAINING") expected = 4;
        CHECK(unreached == expected);
        if (unreached != expected) std::printf("  %s: %d bytes unreached\n", std::string(file).c_str(), unreached);
        CHECK(texts > 0);
    }
}
