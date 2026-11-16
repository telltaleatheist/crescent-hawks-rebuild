#include "game/training_quiz.h"
#include "test.h"

using namespace chr::game;

// Three different parts 0-9, and a repeat costs exactly one more draw:
// the generator ends where the raw numbers, taken mod 10 with repeats
// skipped, first give three different values.
TEST(quiz_draws_three_different_parts) {
    for (int start = 0; start < 64; ++start) {
        Random random = Random::at_town_start();
        for (int i = 0; i < start; ++i) random.next();
        Random raw = random;
        const auto pictures = draw_quiz_parts(random);

        int found[3] = {-1, -1, -1};
        int n = 0;
        while (n < 3) {
            const int p = raw.next() % 10;
            if (p != found[0] && p != found[1]) found[n++] = p;
        }
        for (int q = 0; q < 3; ++q) {
            CHECK(pictures[std::size_t(q)] == found[q]);
            CHECK(pictures[std::size_t(q)] >= 0 && pictures[std::size_t(q)] <= 9);
        }
        CHECK(pictures[0] != pictures[1] && pictures[1] != pictures[2] && pictures[0] != pictures[2]);
        CHECK(raw.state() == random.state());
    }
}
