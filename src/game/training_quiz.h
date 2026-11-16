// The training center's quiz on 'Mech parts (the original's copy
// protection). The rebuild has no protection check (docs/rules.md, rule 3)
// and nothing in it can fail. It still makes the quiz's random draws, so the
// numbers that follow stay the original's (docs/combat.md, "The
// copy-protection check"). The scene itself is shown only with --show-quiz
// (Town::set_show_quiz).
#pragma once

#include <array>
#include <span>

#include "game/random.h"

namespace chr::game {

// The parts the quiz asks about, in order: three different numbers 0-9,
// the parts pictured on BTSTATS.CMP (formats::QuizTables).
using QuizParts = std::array<int, 3>;

// BTECH.EXE 1c67:0b20 (in FUN_1c67_08a8)
// One question's part: a raw number mod 10, drawn again while it is one of
// the parts already `asked`.
int draw_quiz_part(Random& random, std::span<const int> asked);

// BTECH.EXE 1c67:08a8 (Ghidra FUN_1c67_08a8), its draws only
// The quiz's three parts, drawn as the original draws them, one question
// after another.
QuizParts draw_quiz_parts(Random& random);

// The scene while it is shown: the part boxed on BTSTATS.CMP, and its point
// on the 320x200 screen (formats::QuizPart). The renderer draws the box and
// its pointer there.
struct QuizScene {
    int part = 0;
    int x = 0, y = 0;
    bool operator==(const QuizScene&) const = default;
};

}  // namespace chr::game
