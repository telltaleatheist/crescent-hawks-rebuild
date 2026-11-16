// The town's main loop (BTECH.EXE 1000:002d, docs/town_loop.md): walking
// around a map with the screen's text (the party panel at the left, the
// message box over the map, the yes/no question), and the game tick that
// runs after every key and every 11 passes without one. Input comes as keys
// and idle passes, so a script and a player drive it the same way.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "formats/mtp.h"
#include "game/figures.h"
#include "game/map_view.h"
#include "game/money.h"
#include "game/movement.h"
#include "game/random.h"
#include "game/townspeople.h"
#include "game/text.h"
#include "game/text_provider.h"

namespace chr::game {

class KeySource {
public:
    virtual ~KeySource() = default;
    // The next key, or nothing when there are no more (a script has ended).
    virtual std::optional<int> next() = 0;
};

// One step of a script: a key, or `idle` passes of the main loop with no key
// (each one display frame, see game/clock.h).
struct ScriptItem {
    int key = 0;
    int idle = 0;  // > 0: idle passes instead of a key
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

private:
    std::deque<ScriptItem> items_;
};

// Parses a key script: names separated by spaces or commas, each with an
// optional "*N" repeat. Names: up down left right home end pgup pgdn (the
// keypad diagonals), space enter, a single character such as y or n, or
// idle:N for N passes of the main loop without a key.
// Throws std::invalid_argument naming the bad token.
std::deque<ScriptItem> parse_key_script(const std::string& script);

// A character's panel numbers: the three bars under the heading "BDC"
// (bytes 1, 2 and 3 of its 17-byte record at DGROUP:c614) and byte 15, which
// the first bar's warning part is measured from. What they mean is not
// read yet.
struct PanelStats {
    int b = 0, d = 0, c = 0;
    int byte15 = 0;
};

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
    // `texts` supplies every text shown (by ID); it must outlive the Town.
    Town(const formats::MtpMap& map, int map_number, formats::MapPosition start,
         const TextProvider& texts);

    // BTECH.EXE 1000:4cac (Ghidra FUN_1000_4cac)
    // The left panel: the party box (window 3) with the characters and
    // C-Bills, and the box above it (window 4).
    void draw_panel();

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
    // The map as it shows now, animated tiles in their current frame.
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
    // The place the party went into (the walk stops there for now).
    std::optional<int> entered_place() const { return entered_place_; }

private:
    class Question;
    void turn(int key, KeySource& keys);
    // BTECH.EXE 1000:020a..050d (the main loop's game tick)
    void game_tick();
    void record_party_places();
    // BTECH.EXE 1e31:1fdf (Ghidra FUN_1e31_1fdf)
    // "C-Bills:" and the amount, in the party box.
    void draw_cbills();
    // BTECH.EXE 24d3:17c6 (Ghidra FUN_24d3_17c6)
    void open_message_box();
    // BTECH.EXE 1000:1a13 (Ghidra FUN_1000_1a13)
    std::optional<bool> yes_no(bool yes_first, KeySource& keys);

    const formats::MtpMap& map_;
    int map_number_;
    std::string tile_sheet_;
    formats::MapPosition position_;
    const TextProvider& texts_;
    Party party_;
    // BTECH.EXE 1000:4edf: Jason starts with 8, 9, 7 and 80.
    std::array<PanelStats, 8> stats_{{{8, 9, 7, 80}}};
    FigureAnimation animation_;
    TextScreen text_;
    Random random_ = Random::at_town_start();
    Townspeople people_;
    PartyPlaces party_places_{};  // DGROUP:4004/4036 for figures 4-11
    Money money_;
    bool outdoors_ = false;   // DGROUP:d310: false on a town map
    int countdown_ = 0;       // the main loop's [bp-1a]: 0 when it starts (1000:0016)
    std::uint8_t third_ = 0;  // DGROUP:57fe
    int phase_ = 0;           // DGROUP:5800
    int ticks_ = 0;
    std::vector<std::string> log_;
    bool question_open_ = false;
    std::optional<int> entered_place_;
};

}  // namespace chr::game
