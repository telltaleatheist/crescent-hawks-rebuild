#include "game/training_quiz.h"

#include <algorithm>

namespace chr::game {

// BTECH.EXE 1c67:08a8 (Ghidra FUN_1c67_08a8), its draws only
// The original divides the raw number by 10 with a signed division; the raw
// number is 0-255, so that is a plain remainder. The question counter goes up
// only once a new picture is found, so a repeat costs one more draw and
// nothing else.
QuizPictures draw_quiz_pictures(Random& random) {
    QuizPictures asked{};
    for (int question = 0; question < 3; ++question) {
        int picture = 0;
        do {
            picture = random.next() % 10;
        } while (std::find(asked.begin(), asked.begin() + question, picture) !=
                 asked.begin() + question);
        asked[std::size_t(question)] = picture;
    }
    return asked;
}

}  // namespace chr::game
