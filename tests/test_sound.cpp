#include <vector>

#include "game/sound.h"
#include "test.h"

using namespace chr::game;

namespace {

// A made-up list in the original's shape: effect 1 is a note then a kind-2
// entry; effect 2 is two kind-2 entries; effect 3 a kind-3 entry and a
// code past the kinds. Each ends with an entry followed by two 0 words.
const std::vector<int> kLists = {
    500, 1, 2,                          // effect 1: a note
    0x3ea, 2, 10, 11, 12, 13, 14,       //   kind 2
    0x3e9, 0, 0, 1, 1, 1,               //   kind 1, followed by two 0 words: the end
    0x3ea, 1, 20, 21, 22, 23, 24,       // effect 2: kind 2
    0x3ea, 1, 30, 31, 32, 33, 34,       //   kind 2, the last
    500, 0, 0,                          //   the end
    0x3eb, 1, 9, 9, 9, 9, 9,            // effect 3: kind 3
    0x3f0,                              //   a code past the kinds: one word
    500, 0, 0,                          //   the end
    0,
};

}  // namespace

TEST(sound_keeps_the_last_kind_two_words) {
    SoundState s;
    CHECK(s.minus_one_x() == 0 && s.minus_one_y() == 0);  // the image's values
    play_entries(s, kLists, 1);
    CHECK(s.sweep[0] == 10 && s.sweep[3] == 13 && s.sweep[4] == 14);
    play_entries(s, kLists, 2);
    CHECK(s.sweep[0] == 30 && s.minus_one_y() == 33);
    play_entries(s, kLists, 3);  // no kind-2 entry: kept
    CHECK(s.minus_one_y() == 33);
    CHECK(s.minus_one_x() == 0);
}

TEST(sound_past_the_last_effect_plays_nothing) {
    SoundState s;
    play_entries(s, kLists, 4);
    play_entries(s, kLists, 40);
    CHECK(s.minus_one_y() == 0);
    // An entry whose next two words are 0 ends the effect before it plays.
    const std::vector<int> ends_first = {0x3ea, 0, 0, 5, 5, 5, 5, 0};
    play_entries(s, ends_first, 1);
    CHECK(s.minus_one_y() == 0);
}

TEST(sound_codes_are_compared_signed) {
    // 0x8000 counts as a note (3 words), so the kind-2 entry after it is
    // reached as effect 1's second entry.
    const std::vector<int> lists = {0x8000, 1, 1, 0x3ea, 1, 1, 2, 3, 77, 5, 500, 0, 0, 0};
    SoundState s;
    play_entries(s, lists, 1);
    CHECK(s.minus_one_y() == 77);
}
