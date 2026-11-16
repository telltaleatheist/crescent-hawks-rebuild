// Draws the training center quiz's scene (game::QuizScene, --show-quiz):
// BTSTATS.CMP over the whole screen, the line down the picture's left edge,
// and the box round the part asked about with a bar pointing at it from that
// line. How it looks is the renderer's choice; this draws the original's
// (1c67:0a81 and 1c67:09ac, 273d:031c lines and a 273d:01fb box). The text
// (the question, the part names, the menu) is the text screen's.
#pragma once

#include "formats/palette.h"
#include "game/training_quiz.h"
#include "platform/renderer.h"

namespace chr::platform {

bool draw_quiz(Renderer& renderer, const game::QuizScene& scene, const formats::Palette16& palette);

}  // namespace chr::platform
