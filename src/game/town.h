// The town's main loop (BTECH.EXE 1000:002d, docs/town_loop.md): walking
// around a map with the screen's text (the party panel at the left, the
// message box over the map, the yes/no question), and the game tick that
// runs after every key and every 11 passes without one. Input comes as keys
// and idle passes, so a script and a player drive it the same way.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
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
#include "game/terrain.h"
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
    // The same check, handing over the key it took (273d:002f, then
    // 273d:0259): kNoKey when none is waiting.
    static constexpr int kNoKey = 0x10000;
    virtual std::optional<int> waiting_key() = 0;
};

// One step of a script: a key, or `idle` passes of the main loop with no key
// (each one display frame, see game/clock.h).
struct ScriptItem {
    int key = 0;
    int idle = 0;       // > 0: idle passes instead of a key
    int set_flag = -1;  // >= 0: a test aid, set this flag to set_value instead
    int set_value = 0;
    int poke_record = -1;  // >= 0: a test aid, write poke_value to byte poke_byte of this record
    bool poke_character = false;  // a character record (char:), else a 'Mech record (mech:)
    int poke_byte = 0;
    int poke_value = 0;
    bool is_set() const { return set_flag >= 0; }
    bool is_poke() const { return poke_record >= 0; }
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
    std::optional<int> waiting_key() override;
    // Where mech: items go: they are applied as the next key or pass is taken.
    void set_poke(std::function<void(bool character, int record, int byte, int value)> poke) { poke_ = std::move(poke); }

private:
    void apply_pokes();
    std::deque<ScriptItem> items_;
    std::function<void(bool, int, int, int)> poke_;
};

// Parses a key script: names separated by spaces or commas, each with an
// optional "*N" repeat. Names: up down left right home end pgup pgdn (the
// keypad diagonals), space enter, a single character such as y or n, or
// idle:N for N passes of the main loop without a key, or set:NN=VV (hex) to
// set flag NN to VV at that point (a test aid, docs/reference.md). Only the
// town's main loop takes a set: item; a question or the start-up refuses one.
// mech:R:BB=VV (hex) writes VV to byte BB of 'Mech record R wherever the
// next key or pass is taken (a test aid standing in for damage).
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
    // The terrain points at the town's own map.
    Town(const Town&) = delete;
    Town& operator=(const Town&) = delete;

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
    void idle_pass(KeySource* keys = nullptr);

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
    // The XORed boxes still showing: a cell painted over since (a window
    // cleared or framed, text written) shows the paint instead.
    std::vector<XorBox> boxes() const;
    const FightFigures& fight_figures() const { return figs_; }
    const FigureAnimation& figure_animation() const { return animation_; }
    const std::array<formats::MechRecord, 8>& mechs() const { return mechs_; }
    // A test aid: one byte of a 'Mech record (the script's mech: items).
    void poke_mech(int record, int byte, int value) {
        mechs_[std::size_t(record)][std::size_t(byte)] = std::uint8_t(value);
        log_.push_back("test aid: 'Mech record " + std::to_string(record) + " byte " + std::to_string(byte) + " set");
    }
    // A test aid: one byte of a character record (the script's char: items).
    void poke_character(int record, int byte, int value) {
        characters_[std::size_t(record)][std::size_t(byte)] = std::uint8_t(value);
        log_.push_back("test aid: character record " + std::to_string(record) + " byte " + std::to_string(byte) + " set");
    }
    const CombatState& combat() const { return combat_; }
    const MapMarks& marks() const { return marks_; }
    // DGROUP:57fe and 5802: the townspeople move by these.
    int town_ticks() const { return third_; }
    int mission_ticks() const { return mission_ticks_; }
    // The original's bugs: kept by default, each fixable (game/fixes.h).
    void set_fixes(const Fixes& fixes) {
        fixes_ = fixes;
        load_terrain();
    }
    // The land around the party (game/terrain.h).
    const Terrain& terrain() const { return terrain_; }
    const formats::MtpMap& map() const { return map_; }
    // The fight reached a step the rebuild stops at (logged; docs/combat.md).
    bool fight_unfinished() const { return fight_unfinished_; }
    // Whether the townspeople have left for a fight: their figures 16-23
    // (the same memory in the original, F:4004 / 4036 / 3920) are the
    // fight's enemy people until it puts them back.
    bool townspeople_away() const { return townspeople_away_; }

private:
    class Question;
    // Nothing more happens: a question or a place waits for keys that ran
    // out, or the walk stopped at a place it can't run.
    bool stopped() const {
        return question_open_ || place_open_ || start_up_open_ || fight_unfinished_ || quit_ ||
               (entered_place_ && !game_dir_);
    }
    // The start-up's screens, in order (game/startup.cpp).
    enum class Stage { Infocom, Title, Question, Town };
    // Runs the start-up from the keys; false if they ran out.
    bool run_start_up(ScriptKeys& input);
    void new_game_screen();
    void turn(int key, KeySource& keys);
    // BTECH.EXE 1000:020a..050d (the main loop's game tick)
    void game_tick(KeySource* keys);
    void game_over(KeySource& keys);
    void walk_position(formats::MapPosition to);
    bool quit_ = false;
    bool question_waiting_ = false;
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
    void clear_window();
    void count_members();

    // A fight (game/fight.cpp, docs/combat.md). Each returns false if the
    // keys ran out while it waited for one, or the fight went on past what
    // is rebuilt.
    bool walk_in(KeySource& keys);
    bool run_mission(int mode, KeySource& keys);
    // 17dc:0629's own state during a mission's walk.
    struct MissionWalk {
        int mode = 0;
        bool done = false;    // [bp-2]: the mission's task is done
        bool warned = false;  // [bp-e]: "Don't come back here..." shown
        int count = 0;        // [bp-14]: the course's ticks
        std::optional<std::uint8_t> saved_cell;  // [bp-18]: a map cell the mission changed
        std::size_t cell = 0;                    // [bp-4]: that cell, in the map store
        int spot = 0;                            // [bp-22]: mission 1's spot
        bool end = false;                        // [bp-10]: the walk ends
        std::array<bool, 4> bays{};              // [bp-c]: mode 9's bays visited
        int found = 0;                           // [bp-24]: mode 9's bays tried
    };
    bool jail_break(KeySource& keys);
    bool failed_start();
    bool jail_bays(MissionWalk& w, KeySource& keys);
    // The bars as the panel last drew them, while the records they came from
    // are hidden (the jail break: docs/combat.md 1.7).
    std::optional<std::vector<StatBar>> held_bars_;
    // F:398e: the figure drawing adds the jail's four bays.
    bool bays_drawn_ = false;
    bool mission_walk(MissionWalk& w, KeySource& keys);
    void mission_tick();
    void mission_draw();
    void draw_marks();
    void party_to_jason();
    bool load_town_map(int number, int grid = 4);
    bool raid_lines(bool over, KeySource& keys);
    int raid_line_ = 0;  // [bp-8] of 203b:000a: the raid's story lines shown
    bool mission_step_done(MissionWalk& w, KeySource& keys);
    void mission_results(const MissionWalk& w);
    void move_townspeople();
    // Sets byte `index` of the map store (P:101d) and has the window built
    // again.
    void set_map_cell(std::size_t index, int tile, bool rebuild = true);
    void place_mission(int mechs, int people);
    bool fight(int kind, KeySource& keys);
    bool alley_fight(KeySource& keys);
    bool new_member(int skill, KeySource& keys);
    bool offer_weapon(int weapon, KeySource& keys);
    bool crew_screen(int start, KeySource& keys);
    void fight_ends();
    bool random_encounter(KeySource& keys);
    bool place_encounter();
    bool encounter_end(bool fought, KeySource& keys);
    void reload_areas();
    bool after_fight_steps(KeySource& keys);
    bool loot(KeySource& keys);
    formats::MapPosition encounter_entry_;
    std::array<std::uint8_t, 9> encounter_blocks_{};
    // The orders menu: 0 Begin Fight, 1 Flee; nothing when the keys ran
    // out or an order is chosen that the rebuild stops at.
    std::optional<int> orders_menu(Combat& combat, KeySource& keys);
    void show_unit(Combat& combat, int fig, bool redraw);
    void plan_shown(Combat& combat, int fig);
    bool move_order(Combat& combat, int fig, int type, KeySource& keys);
    bool weapons_order(Combat& combat, int fig, KeySource& keys);
    bool kick_order(Combat& combat, int fig, KeySource& keys);
    bool scan_order(Combat& combat, int fig, KeySource& keys);
    bool scan_list(Combat& combat, int fig, int base, bool mechs, KeySource& keys);
    bool pick_target(Combat& combat, int fig, int w, int slot, KeySource& keys);
    // A 'Mech record's name (the text of the template it was made from).
    std::optional<std::string> mech_name_id(int record) const;
    int picked_ = 0x0c;  // DG:2b20: the target picker's figure, kept between calls
    // 287f:2b87 over the map: a box XORed onto the screen. Where it
    // overlaps another, the cells get both colours XORed; on text, the
    // text's colours.
    void xor_box(const XorBox& box);
    // The rounds, the party's side ordered by the player or planned by the
    // computer, until the fight ends; then its ending.
    bool fight_rounds(int kind, KeySource& keys);
    bool press_key(KeySource& keys);
    // 273d:06c3: the page made the screen over the map's columns.
    void show_page();
    Combat combat_rules();
    // game/training_quiz.cpp: the quiz's scene (--show-quiz).
    bool show_quiz(KeySource& keys);
    static constexpr int kQuizMenu = 0x25;
    void build_window();
    void run_map_loads();
    bool enemy_people_run(KeySource& keys, bool& over);
    bool anyone_hurt() const;
    bool heal(int doctor, KeySource& keys);
    bool repair(KeySource& keys);
    bool salvage(KeySource& keys);
    int slot_location_structure(int m, int slot) const;
    std::optional<StepResult> step_party(Step step, KeySource& keys);
    MechParty active_mechs() const;
    // Which of 'Mech records 0-3 exist (byte 0 not ff).
    std::array<bool, 4> mech_records() const {
        std::array<bool, 4> exist{};
        for (int m = 0; m < 4; ++m) exist[std::size_t(m)] = mechs_[std::size_t(m)][0] != 0xff;
        return exist;
    }
    std::vector<Figure> walk_in_figures();
    Figure party_mech_figure(int m, int k) const;
    std::vector<Figure> fight_figures_drawn(bool marks = true);
    bool print_unit_heading(int fig, bool agent_check, bool details);
    bool print_condition(int character);
    bool on_foot(int character) const;
    // The screen as it was last drawn, while the program draws nothing new
    // over the map (the questions before a fight).
    struct Frozen {
        MapView view;
        std::vector<Figure> figures;
    };

    formats::MtpMap map_;  // the store's map blocks (P:101d), which the missions change
    int map_number_;
    std::string tile_sheet_;
    formats::MapPosition position_;
    const TextProvider& texts_;
    const formats::Tables& tables_;
    // The party: characters 0-7 whose record is in use (byte 0, the name
    // number, not ff), as the original tests it everywhere.
    Party party() const {
        Party p;
        for (int i = 0; i < 8; ++i) p.present[std::size_t(i)] = characters_[std::size_t(i)][character::kName] != character::kEmpty;
        return p;
    }
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
    // F:d452: the first bytes of 'Mech records 0-3 while they are set aside
    // (mode 9, the garage); a letter means a 'Mech is held there.
    std::array<std::uint8_t, 4> held_mechs_ = {0xff, 0xff, 0xff, 0xff};  // ff at a new game (1000:4dc7)
    // F:d456: the next character name number, 1 at a new game (1000:4ebc).
    std::uint8_t next_name_ = 1;
    // F:c614: the sixteen character records.
    std::array<CharacterRecord, 16> characters_ = new_game_characters();
    FightFigures figs_;
    // F:d562: the characters' sprite bases (figures 4-11 of F:d55e).
    std::array<int, 8> people_sprite_base() const {
        std::array<int, 8> b{};
        for (int i = 0; i < 8; ++i) b[std::size_t(i)] = figs_.sprite_base[std::size_t(4 + i)];
        return b;
    }
    CombatState combat_;
    MapMarks marks_;
    std::vector<Figure> drawn_marks_;  // what 1000:2a93 drew last
    Fixes fixes_;
    Terrain terrain_{tables_, fixes_};
    // The terrain as the map load leaves it (1000:4dc7: 287f:104e for the
    // party's area, 1000:2da8 into grid 4, 287f:1da8).
    void load_terrain();
    int messages_ = 2;       // DG:2e38: combat messages (0 none, 1 brief, 2 verbose)
    bool graphics_ = true;   // DG:2e3a: see combat graphics
    bool computer_ = false;  // F:009e: the computer fights for you
    bool no_computer_order_ = false;  // F:0090: no Computer order (mission 4)
    bool raid_ = false;               // F:3772: the Kurita raid
    int path_end_col_ = 0, path_end_row_ = 0;  // F:3778/377a: the cell where the shown path ends
    std::optional<Frozen> frozen_;  // the map as the screen shows it
    std::optional<Frozen> last_drawn_;
    bool encounter_frozen_ = false;  // frozen_ is the encounter's: the tick's drawing ends it  // the map as the last tick drew it, outdoors
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
    std::uint32_t boxes_since_ = 0;  // the text layer's paint clock when boxes_ was last settled
    void settle_boxes();
    bool party_box_shown_ = true;
    bool fight_unfinished_ = false;
    MechParty step_mechs_;            // the step check's 'Mechs, read again after a visit
    formats::MapPosition window_at_;  // where 287f:1314 last built the window
    bool window_stale_ = false;       // the map changed since
    int mission_ticks_ = 0;  // DG:5802: the missions' walk's ticks, 0-3
    bool townspeople_away_ = false;
    bool show_quiz_ = false;
    std::optional<QuizScene> quiz_;
};

}  // namespace chr::game
