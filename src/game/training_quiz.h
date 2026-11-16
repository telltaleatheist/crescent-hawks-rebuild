// The training center's 'Mech-picture quiz (the original's copy protection).
// The rebuild has no protection check (docs/rules.md, rule 3) and nothing in
// it can fail. It still makes the quiz's random draws, so the numbers that
// follow stay the original's (docs/combat.md, "The copy-protection check").
#pragma once

#include <array>

#include "game/random.h"

namespace chr::game {

// The pictures the quiz would ask about, in order: three different numbers
// 0-9, indexes into the pictures of BTSTATS.CMP.
using QuizPictures = std::array<int, 3>;

// BTECH.EXE 1c67:08a8 (Ghidra FUN_1c67_08a8), its draws only
// Picks the quiz's three pictures as the original does: one raw number mod
// 10 per question, drawn again while it repeats an earlier question's. The
// original then shows the picture and a menu of names for each; how the
// rebuild presents the scene is not decided yet, so nothing is shown.
QuizPictures draw_quiz_pictures(Random& random);

}  // namespace chr::game
