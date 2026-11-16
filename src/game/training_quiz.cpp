#include "game/training_quiz.h"

#include <algorithm>
#include <string>

#include "formats/rules.h"
#include "game/text_provider.h"
#include "game/town.h"

namespace chr::game {

// BTECH.EXE 1c67:0b20 (in FUN_1c67_08a8)
// The original divides the raw number by 10 with a signed division; the raw
// number is 0-255, so that is a plain remainder. The question counter goes up
// only once a new part is found, so a repeat costs one more draw and nothing
// else.
int draw_quiz_part(Random& random, std::span<const int> asked) {
    int part = 0;
    do {
        part = random.next() % 10;
    } while (std::find(asked.begin(), asked.end(), part) != asked.end());
    return part;
}

QuizParts draw_quiz_parts(Random& random) {
    QuizParts asked{};
    for (std::size_t q = 0; q < asked.size(); ++q) asked[q] = draw_quiz_part(random, std::span(asked).first(q));
    return asked;
}

// BTECH.EXE 1c67:08a8 (Ghidra FUN_1c67_08a8), shown (--show-quiz)
// The scene as the original lays it out, from the player's own files, with
// no check (docs/rules.md, rule 3): each menu starts on the right part and
// any answer is taken. Nothing here sets flag 14. False if the keys ran out
// (the scene stays on the screen).
bool Town::show_quiz(KeySource& keys) {
    // 1000:28cc asks for the disk with BTSTATS.CMP; there are no disks. The
    // picture is loaded and unpacked off the screen.
    const TextScreen before = text_;
    // 1c67:0935: the message in the big box (window 6, frame style 0), and a key.
    text_.select_window(6);
    text_.clear_window();
    text_.frame(0);
    print_id("quiz.message");
    if (!read_key(keys)) return false;

    QuizParts asked{};
    for (std::size_t q = 0; q < asked.size(); ++q) {
        // 1c67:0a81: the picture over the whole screen (the renderer draws
        // it while quiz_ is set, with the line down its left edge), the
        // question in the left strip (window 8) and the 20 part names from
        // row 4, white on black (273d:00d5).
        text_.clear_all();
        text_.select_window(8);
        text_.clear_window();
        print_id("quiz.question");
        for (int i = 0; i < formats::kQuizPartNames; ++i)
            text_.put_text(lookup(texts_, "quiz.part." + std::to_string(i)), 0, 4 + i, 15, 0);
        // 1c67:0b20: this question's part; 1c67:09ac boxes it.
        const int part = draw_quiz_part(random_, std::span(asked).first(q));
        asked[q] = part;
        const auto& p = rules_->quiz[std::size_t(part)];
        quiz_ = QuizScene{part, p.x, p.y};
        // 1c67:0a59: menu 0x25. Where the original compares the answer with
        // DG:28ec (1c67:0a6a), the rebuild has no check: the bar starts on
        // the right part, and whatever is chosen is taken. (The menu still
        // remembers the item chosen, as the original's does.)
        const auto chosen = menu(kQuizMenu, keys, p.answer);
        if (!chosen) return false;
        log_.push_back("training quiz: part " + std::to_string(part) + ", chose " + std::to_string(*chosen));
    }
    // 1c67:0b3b: 287f:1fbe sets the screen back for the town; the box and
    // the picture are gone. (The panel is redrawn after, as without the scene.)
    quiz_.reset();
    text_ = before;
    return true;
}

}  // namespace chr::game
