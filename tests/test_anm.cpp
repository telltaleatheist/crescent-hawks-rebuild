#include "formats/anm.h"
#include "test.h"

using namespace chr::formats;

namespace {

// A header: the frame list from offset 0, the speed at 0x32.
std::vector<std::uint8_t> header(std::initializer_list<std::uint8_t> list, std::uint8_t speed) {
    std::vector<std::uint8_t> f(0x33, 0);
    int i = 0;
    for (const auto b : list) f[i++] = b;
    f[0x32] = speed;
    return f;
}

}  // namespace

TEST(anm_long_run_and_delay) {
    // One frame: 3872 bytes of 0x12 (00 hh ll v). Entry 0x21 names the delay
    // byte at offset 0, which is the entry itself: 33 * 4 * 3 / 4 = 99 ticks.
    auto f = header({0x21}, 4);
    f.insert(f.end(), {0x00, 0x0f, 0x20, 0x12});
    const auto p = decode_anm(f);
    CHECK(p.frames.size() == 1 && p.delays == std::vector<int>{99});
    CHECK(p.frames[0][0] == 1 && p.frames[0][1] == 2 && p.frames[0][88 * 88 - 1] == 2);
}

TEST(anm_frames_xor_onto_the_last) {
    // Two frames: all 0x12, then two literal bytes (10 01) and a run of 3870
    // zeros, which leave the rest as it was.
    auto f = header({0x21, 0x22}, 4);
    f.insert(f.end(), {0x00, 0x0f, 0x20, 0x12, 0x02, 0x10, 0x01, 0x00, 0x0f, 0x1e, 0x00});
    const auto p = decode_anm(f);
    CHECK(p.frames.size() == 2);
    CHECK(p.frames[1][0] == 0 && p.frames[1][1] == 2);  // 12 ^ 10
    CHECK(p.frames[1][2] == 1 && p.frames[1][3] == 3);  // 12 ^ 01
    CHECK(p.frames[1][4] == 1 && p.frames[1][5] == 2);
    CHECK(p.delays == std::vector<int>({99, 102}));     // 0x22 = 34: 34 * 4 * 3 / 4
}

TEST(anm_truncated_file_throws) {
    // A run byte n >= 0x80 is 256 - n copies; a file that stops inside a
    // frame is an error.
    auto f = header({0x21}, 1);
    f.insert(f.end(), {0xfe, 0x33});
    bool threw = false;
    try {
        decode_anm(f);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}
