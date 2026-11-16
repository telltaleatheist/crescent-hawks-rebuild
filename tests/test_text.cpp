#include <array>
#include <vector>

#include "formats/cells.h"
#include "formats/exe_data.h"
#include "formats/exepack.h"
#include "formats/game_files.h"
#include "formats/sha256.h"
#include "game/text.h"
#include "test.h"

using namespace chr;
using namespace chr::game;

TEST(sha256_known_values) {
    CHECK(formats::sha256_hex({}) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string abc = "abc";
    CHECK(formats::sha256_hex(std::span(reinterpret_cast<const std::uint8_t*>(abc.data()), 3)) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Two blocks of padding.
    const std::string long_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(formats::sha256_hex(std::span(reinterpret_cast<const std::uint8_t*>(long_text.data()),
                                        long_text.size())) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(font_character_bits) {
    std::vector<std::uint8_t> font(1024, 0);
    font[65 * 8 + 0] = 0x81;  // 'A', top row: leftmost and rightmost pixels
    const auto a = formats::font_character(font, 65);
    CHECK(a[0] == 1 && a[7] == 1 && a[1] == 0 && a[8] == 0);
}

TEST(window_frame_pieces) {
    TextScreen t;
    t.select_window(3);  // (1, 13) 11x11
    t.frame(3);
    CHECK(t.cell(0, 12).kind == TextCell::Kind::Frame && t.cell(0, 12).code == 2);   // top-left
    CHECK(t.cell(12, 12).code == 3);                                                 // top-right
    CHECK(t.cell(0, 24).code == 6 && t.cell(12, 24).code == 7);                      // bottom corners
    CHECK(t.cell(5, 12).code == 4 && t.cell(5, 24).code == 4);                       // top, bottom
    CHECK(t.cell(0, 18).code == 5 && t.cell(12, 18).code == 5);                      // sides
    CHECK(t.cell(5, 18).kind == TextCell::Kind::Empty);                              // inside untouched
}

TEST(printer_wraps_words) {
    TextScreen t;
    t.select_window(7);  // (14, 10) 25x5
    t.clear_window();
    t.print("The water is too deep that way.");
    CHECK(t.row_text(10).substr(14, 25) == "The water is too deep    ");
    CHECK(t.row_text(11).substr(14, 25) == "that way.                ");
}

TEST(printer_newline_and_colours) {
    TextScreen t;
    t.select_window(7);
    t.clear_window();
    const std::array<std::uint8_t, 8> s = {'A', 0x0d, 0x06, 4, 0x02, 14, 'B', 0};
    t.print(std::span<const std::uint8_t>(s));
    CHECK(t.row_text(10).substr(14, 2) == "A ");
    CHECK(t.cell(14, 11).code == 'B' && t.cell(14, 11).fg == 4 && t.cell(14, 11).bg == 14);
}

TEST(printer_scrolls_at_the_bottom) {
    TextScreen t;
    t.select_window(7);
    t.clear_window();
    t.print("1\r2\r3\r4\r5\r6");
    CHECK(t.row_text(10).substr(14, 1) == "2");
    CHECK(t.row_text(14).substr(14, 1) == "6");
}

TEST(exe_rejects_other_programs) {
    std::vector<std::uint8_t> junk(4096, 0);
    junk[0] = 'M';
    junk[1] = 'Z';
    bool threw = false;
    try {
        formats::OriginalExe exe(junk);
    } catch (const formats::FormatError& e) {
        threw = std::string(e.what()).find("DOS 1.03") != std::string::npos;
    }
    CHECK(threw);
}

TEST(exe_unpacks_like_the_tool_and_has_the_font) {
    const auto dir = chrtest::game_dir();
    if (dir.empty()) {
        chrtest::skip("no game folder (set CHR_GAME_DIR)");
        return;
    }
    const auto path = formats::find_game_file(dir, "BTECH.EXE");
    CHECK(path.has_value());
    if (!path) return;
    const auto unpacked = formats::unexepack(formats::read_file(*path));
    CHECK(formats::sha256_hex(unpacked) == formats::kSupportedUnpackedSha256);
    const auto exe = formats::OriginalExe::load(dir);
    CHECK(exe.get("font").size() == 1024);
    // 'A' has pixels; the space has none.
    const auto a = formats::font_character(exe.get("font"), 'A');
    const auto space = formats::font_character(exe.get("font"), ' ');
    int lit_a = 0, lit_space = 0;
    for (int i = 0; i < 64; ++i) {
        lit_a += a[i];
        lit_space += space[i];
    }
    CHECK(lit_a > 8 && lit_space == 0);
    CHECK(exe.text("text.too_deep") == "The water is too deep that way.");
    CHECK(exe.text("name.0") == "Jason");
    CHECK(exe.text("text.party_heading") == "BDC");
}
