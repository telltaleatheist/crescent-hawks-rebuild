// The town's main loop (BTECH.EXE 1000:002d, docs/town_loop.md): walking
// around a map with the screen's text (the party panel at the left, the
// message box over the map, the yes/no question), and the game tick that
// runs after every key and every 11 passes without one. Input comes as keys
// and idle passes, so a script and a player drive it the same way.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "formats/tables.h"
#include "game/asset_id.h"
#include "formats/mtp.h"
#include "formats/rules.h"
#include "game/combat.h"
#include "game/fight.h"
#include "game/fixes.h"
#include "game/training_quiz.h"
#include "game/figures.h"
#include "game/flags.h"
#include "game/map_view.h"
#include "game/money.h"
#include "game/movement.h"
#include "game/random.h"
#include "game/records.h"
#include "game/shown.h"
#include "game/townspeople.h"
#include "game/text.h"
#include "game/text_provider.h"

namespace chr::game {

class KeySource {
public:
    virtual ~KeySource() = default;
    // The next key, or nothing when there are no more (a script has ended).
    virtual std::optional<int> next() = 0;
    // A "is a key waiting?" check that does not wait (273d:002f): true
    // with a key, which is taken; false when none is waiting. Nothing when
    // the keys have run out.
    virtual std::optional<bool> poll() = 0;
};

// One step of a script: a key, or `idle` passes of the main loop with no key
// (each one display frame, see game/clock.h).
struct ScriptItem {
    int key = 0;
    int idle = 0;       // > 0: idle passes instead of a key
    int set_flag = -1;  // >= 0: a test aid, set this flag to set_value instead
    int set_value = 0;
    bool is_set() const { return set_flag >= 0; }
    bool operator==(const ScriptItem&) const = default;
};

class ScriptKeys : public KeySource {
public:
    explicit ScriptKeys(std::deque<ScriptItem> items) : items_(std::move(items)) {}
    // The next item, key or idle.
    std::optional<ScriptItem> next_item();
    // The next key: a question's blocking read skips idle items (no time
    // passes for the game while it waits there).
    std::optional<int> next() override;
    // An idle item is one pass with no key waiting; a key is waiting.
    std::optional<bool> poll() override;

private:
    std::deque<ScriptItem> items_;
};

// Parses a key script: names separated by spaces or commas, each with an
// optional "*N" repeat. Names: up down left right home end pgup pgdn (the
// keypad diagonals), space enter, a single character such as y or n, or
// idle:N for N passes of the main loop without a key, or set:NN=VV (hex) to
// set flag NN to VV at that point (a test aid, docs/reference.md). Only the
// town's main loop takes a set: item; a question or the start-up refuses one.
// Throws std::invalid_argument naming the bad token.
std::deque<ScriptItem> parse_key_script(const std::string& script);

// A bar of the panel, beside a character's name (1000:4bc1): `value` pixels
// of 12 filled from the bottom, the top `warning` of them in another colour.
// Placed by text cell (the bar's top-left corner); the renderer draws it.
struct StatBar {
    int col = 0, row = 0;
    int value = 0;
    int warning = 0;
    bool operator==(const StatBar&) const = default;
};

class Town {
public:
    // `texts` supplies every text shown (by ID), `tables` the program's small
    // tables (imported, formats/tables.h); both must outlive the Town.
    Town(const formats::MtpMap& map, int map_number, formats::MapPosition start,
         const TextProvider& texts, const formats::Tables& tables);

    // Begins with the program's start-up (game/startup.cpp): the Infocom
    // screen, the title and the first-time question, before the town. Call
    // before run().
    void start_up();
    // A full-screen picture shown instead of the town (the Infocom screen,
    // the title), while the start-up shows one.
    std::optional<AssetId> full_screen() const;
    // The keys ran out during the start-up, which waits for one (or, on the
    // Infocom screen, for time to pass too).
    bool start_up_open() const { return start_up_open_; }

    // BTECH.EXE 1000:4cac (Ghidra FUN_1000_4cac)
    // The left panel: the party box (window 3) with the characters and
    // C-Bills, and the box above it (window 4). `clear`: the party box is
    // cleared after its frame (4cac's argument). Window 4 is left selected.
    void draw_panel(bool clear = false);

    // Takes keys and idle passes until the source runs out.
    void run(ScriptKeys& input);
    // One key: a turn, then a game tick. A question asked during the turn
    // reads its answer from `more` (and stays open if it has none).
    void key(int k, KeySource& more);
    // BTECH.EXE 1000:01f4..020a (the main loop's idle half)
    // One pass of the main loop without a key: it waits a frame, and every
    // 11th pass since the last game tick (the first pass included) runs one.
    void idle_pass();

    formats::MapPosition position() const { return position_; }
    // The map as it shows now, animated tiles in their current frame (or as
    // last drawn, while the program draws nothing new: the fight's questions).
    MapView view() const;
    // The figures on the map, in drawing order: the party, then the
    // townspeople out walking.
    std::vector<Figure> figures() const;
    const Townspeople& townspeople() const { return people_; }
    const Random& random() const { return random_; }
    // The panel's bars, three per character shown.
    std::vector<StatBar> bars() const;
    const Money& money() const { return money_; }
    // Game ticks run so far, and the tile animation's frame (DGROUP:5800).
    int ticks() const { return ticks_; }
    int animation_phase() const { return phase_; }
    const TextScreen& text() const { return text_; }
    // A short line per turn, for tests and --script output.
    const std::vector<std::string>& log() const { return log_; }
    // The script ended in the middle of the yes/no question.
    bool question_open() const { return question_open_; }
    // Stopped at a computer-fought fight's once-a-round "is a key waiting?"
    // (203b:1015): an idle pass answers no.
    bool poll_open() const { return poll_open_; }
    // What the screen showed while the logic ran (game/shown.h), and the
    // sprites and line pixels it shows now over the map.
    const std::vector<ShownEvent>& events() const { return events_; }
    const std::vector<ShownSprite>& sprites() const { return screen_sprites_; }
    const std::vector<ShownPixel>& pixels() const { return screen_pixels_; }
    // The last place the party went into (its entrance number).
    std::optional<int> entered_place() const { return entered_place_; }
    // The keys ran out inside a place, while its script waited for one.
    bool place_open() const { return place_open_; }
    // The place's picture, while one is shown (it shows as it ends: the
    // asset {"O<n>.ANM", 0xffff}, docs/formats/anm.md).
    std::optional<PlacePicture> picture() const { return picture_; }
    // Where the player's .BLD files are. Without it the walk stops at an
    // entrance after "yes".
    void set_game_dir(std::filesystem::path dir) { game_dir_ = std::move(dir); }
    const Flags& flags() const { return flags_; }
    // A test aid (--set-flag): set flag `f` to `v` when the town starts.
    void set_flag_when_town_starts(int f, int v) { pending_flags_.emplace_back(f, v); }
    const std::array<int, formats::kMenuCount>& menu_last() const { return menu_last_; }
    const std::array<CharacterRecord, 16>& characters() const { return characters_; }

    // Shows the training center's quiz (--show-quiz, off by default): the
    // scene from the player's files, with no check (game/training_quiz.h).
    // It needs the rules (the parts' places); without them it is not shown.
    void set_show_quiz(bool on) { show_quiz_ = on; }
    // The quiz scene, while it shows: the renderer draws BTSTATS.CMP and the
    // part's box instead of the town, then the text.
    std::optional<QuizScene> quiz() const { return quiz_; }

    // The rule tables (the 'Mech templates and the combat tables): a fight
    // needs them. Without them a fight stops where it would begin.
    void set_rules(const formats::Rules* rules) { rules_ = rules; }
    // Boxes XORed onto the screen over the map (the orders screen's mark on
    // the selected unit), drawn last.
    const std::vector<XorBox>& boxes() const { return boxes_; }
    const FightFigures& fight_figures() const { return figs_; }
    const FigureAnimation& figure_animation() const { return animation_; }
    const std::array<formats::MechRecord, 8>& mechs() const { return mechs_; }
    const CombatState& combat() const { return combat_; }
    const MapMarks& marks() const { return marks_; }
    // The original's bugs: kept by default, each fixable (game/fixes.h).
    void set_fixes(const Fixes& fixes) { fixes_ = fixes; }
    // The fight reached a step the rebuild stops at (logged; docs/combat.md).
    bool fight_unfinished() const { return fight_unfinished_; }

private:
    class Question;
    // Nothing more happens: a question or a place waits for keys that ran
    // out, or the walk stopped at a place it can't run.
    bool stopped() const {
        return question_open_ || place_open_ || start_up_open_ || fight_unfinished_ ||
               (entered_place_ && !game_dir_);
    }
    // The start-up's screens, in order (game/startup.cpp).
    enum class Stage { Infocom, Title, Question, Town };
    // Runs the start-up from the keys; false if they ran out.
    bool run_start_up(ScriptKeys& input);
    void new_game_screen();
    void turn(int key, KeySource& keys);
    // BTECH.EXE 1000:020a..050d (the main loop's game tick)
    void game_tick();
    void record_party_places();
    // The map at the party's position now.
    MapView live_view() const;
    // BTECH.EXE 1e31:1fdf (Ghidra FUN_1e31_1fdf)
    // "C-Bills:" and the amount, in the party box.
    void draw_cbills();
    // BTECH.EXE 24d3:17c6 (Ghidra FUN_24d3_17c6)
    void open_message_box();
    // BTECH.EXE 1000:1a13 (Ghidra FUN_1000_1a13)
    std::optional<bool> yes_no(bool yes_first, KeySource& keys);

    // The places (game/place.cpp, docs/scripts.md). Each returns false if
    // the keys ran out while it waited for one (place_open_ is then set).
    bool visit_place(int entrance, KeySource& keys);
    bool run_script(int file, KeySource& keys);
    // `start`: the item the bar starts on instead of the remembered one (the
    // quiz scene's right part); the item chosen is remembered as usual.
    std::optional<int> menu(int m, KeySource& keys, std::optional<int> start = std::nullopt);
    bool special(int n, KeySource& keys);
    bool talk(KeySource& keys);
    void show_picture(int number, int mode);
    // The screen as it is, held `frames` display frames (an event).
    void hold_screen(int frames);
    std::optional<int> read_key(KeySource& keys);
    void print_id(const std::string& id);
    void count_members();

    // A fight (game/fight.cpp, docs/combat.md). Each returns false if the
    // keys ran out while it waited for one, or the fight went on past what
    // is rebuilt.
    bool walk_in(KeySource& keys);
    bool run_mission(int mode, KeySource& keys);
    void place_mission(int mechs, int people);
    bool fight(int kind, KeySource& keys);
    bool orders_menu(KeySource& keys);
    // 203b:056b..1037 with the computer fighting for the party: the rounds
    // until one side is beaten, then the fight's ending.
    bool fight_rounds(int kind, KeySource& keys);
    Combat combat_rules();
    // game/training_quiz.cpp: the quiz's scene (--show-quiz).
    bool show_quiz(KeySource& keys);
    static constexpr int kQuizMenu = 0x25;
    std::vector<Figure> walk_in_figures();
    std::vector<Figure> fight_figures_drawn();
    void print_unit_heading(int fig);
    bool on_foot(int character) const;
    // The screen as it was last drawn, while the program draws nothing new
    // over the map (the questions before a fight).
    struct Frozen {
        MapView view;
        std::vector<Figure> figures;
    };

    const formats::MtpMap& map_;
    int map_number_;
    std::string tile_sheet_;
    formats::MapPosition position_;
    const TextProvider& texts_;
    const formats::Tables& tables_;
    Party party_;
    FigureAnimation animation_{tables_};
    TextScreen text_{tables_};
    Random random_ = Random::at_town_start();
    Townspeople people_;
    PartyPlaces party_places_{};  // F:4004/4036 for figures 4-11
    Money money_;
    // Outdoors (F:d310) is flag 04: clear on a town map.
    bool outdoors() const { return flags_[flag::kOutdoors] != 0; }
    int countdown_ = 0;       // the main loop's [bp-1a]: 0 when it starts (1000:0016)
    std::uint8_t third_ = 0;  // DGROUP:57fe
    int phase_ = 0;           // DGROUP:5800
    int ticks_ = 0;
    std::vector<std::string> log_;
    bool question_open_ = false;
    std::optional<int> entered_place_;
    bool place_open_ = false;
    Flags flags_ = new_game_flags();
    std::vector<std::pair<int, int>> pending_flags_;
    Stage stage_ = Stage::Town;
    int infocom_frames_ = 0;  // frames the Infocom screen has been up
    bool start_up_open_ = false;
    // The menu table (385b:0092, imported: tables.json, menus): the items
    // count changes at run time (talk sets menu 20's), and each menu
    // remembers its last item (+6).
    std::array<formats::MenuRecord, formats::kMenuCount> menus_ = tables_.menus;
    std::array<int, formats::kMenuCount> menu_last_{};
    std::optional<PlacePicture> picture_;
    int place_number_ = 0;  // F:4584: the entrance last entered
    std::optional<std::filesystem::path> game_dir_;

    // The fight's state (game/fight.h).
    const formats::Rules* rules_ = nullptr;
    // F:c724 + 0x7d i: no 'Mech in any record at a new game (byte 0 ff).
    std::array<formats::MechRecord, 8> mechs_ = [] {
        std::array<formats::MechRecord, 8> m{};
        for (auto& r : m) r[0] = 0xff;
        return m;
    }();
    // F:c614: the sixteen character records.
    std::array<CharacterRecord, 16> characters_ = new_game_characters();
    FightFigures figs_;
    CombatState combat_;
    MapMarks marks_;
    Fixes fixes_;
    int messages_ = 2;       // DG:2e38: combat messages (0 none, 1 brief, 2 verbose)
    bool graphics_ = true;   // DG:2e3a: see combat graphics
    bool computer_ = false;  // F:009e: the computer fights for you
    std::optional<Frozen> frozen_;  // the map as the screen shows it
    // With combat graphics: the page drawn but not shown yet, the sprites on
    // it and on the screen, the line pixels on the screen; and every moment
    // shown so far (game/shown.h).
    std::optional<Frozen> page_;
    std::vector<ShownSprite> page_sprites_, screen_sprites_;
    std::vector<ShownPixel> screen_pixels_;
    std::vector<ShownEvent> events_;
    int message_speed_ = 2;  // DG:015e
    bool poll_open_ = false;
    std::vector<XorBox> boxes_;
    bool party_box_shown_ = true;
    bool fight_unfinished_ = false;
    bool show_quiz_ = false;
    std::optional<QuizScene> quiz_;
};

}  // namespace chr::game
