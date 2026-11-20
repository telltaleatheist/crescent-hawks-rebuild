// The program's start-up, from the first picture to the town
// (docs/startup.md). The hardware questions the original asks first in text
// mode (the graphics adapter, the disk drives) belong to the platform and
// are not rebuilt.
#include "game/town.h"

namespace chr::game {

namespace {

// BTECH.EXE 1527:01e6: the Infocom screen stays up for at most 700 display
// frames (one 273d:0006 wait each), or until a key.
constexpr int kInfocomFrames = 700;

}  // namespace

void Town::start_up() { stage_ = Stage::Infocom; }

std::optional<AssetId> Town::full_screen() const {
    if (stage_ == Stage::Infocom) return assets::kInfocomLogo;
    if (stage_ == Stage::Title) return assets::kTitleScreen;
    return screen_picture_;
}

// BTECH.EXE 1000:4dc7 (Ghidra FUN_1000_4dc7), as far as the screen goes: a
// new game's panel. Then 1000:5154..51b6: the notice in window 6 and the
// first-time question.
void Town::new_game_screen() {
    draw_panel();
    text_.select_window(6);
    text_.clear_window();
    text_.frame(0);
    for (const char* id : {"intro.notice.0", "intro.notice.1", "intro.notice.2", "intro.notice.3",
                           "intro.first_time"})
        print_id(id);
}

// BTECH.EXE 1527:0131..0216 (the Infocom screen, in Ghidra FUN_1527_0044),
// 1000:46a7 (the title), and 1000:50c8 (Ghidra FUN_1000_50c8):
// the title's tune and the questions.
bool Town::run_start_up(ScriptKeys& input) {
    while (stage_ == Stage::Infocom) {
        const auto item = input.next_item();
        if (!item) return false;
        if (item->is_set()) throw std::invalid_argument("a set: item in the start-up");
        if (item->idle == 0) {
            stage_ = Stage::Title;  // a key ends it (and is not used for anything else)
        } else {
            infocom_frames_ += item->idle;
            if (infocom_frames_ >= kInfocomFrames) stage_ = Stage::Title;
        }
    }
    if (stage_ == Stage::Title) {
        // 1000:476d plays the title's tune (WWOODBT.SIF) until a key; if the
        // tune ends first, the demo plays (DEMOFILE). docs/startup.md.
        if (!input.next()) return false;
        log_.push_back("title");
        stage_ = Stage::Question;
        new_game_screen();
    }
    // "Is this your first time playing?" Yes goes to the town; No asks
    // whether to load a saved game.
    const auto first = yes_no(true, input);
    if (!first) return false;
    if (!*first) {
        print_id("intro.load_old_game");
        const auto load = yes_no(true, input);
        if (!load) return false;
        // 1000:32b3 loads a saved game (docs/startup.md).
        if (*load) log_.push_back("load a saved game: not rebuilt, a new game starts");
    }
    log_.push_back("new game");
    stage_ = Stage::Town;
    return true;
}

}  // namespace chr::game
