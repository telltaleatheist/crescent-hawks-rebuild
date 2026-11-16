// Walking around a map with the screen's text: the party panel at the left,
// the message box over the map, the yes/no question. Keys come from a
// KeySource, so a script and a player drive it the same way.
#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "formats/exe_data.h"
#include "formats/mtp.h"
#include "game/map_view.h"
#include "game/movement.h"
#include "game/text.h"

namespace chr::game {

class KeySource {
public:
    virtual ~KeySource() = default;
    // The next key, or nothing when there are no more (a script has ended).
    virtual std::optional<int> next() = 0;
};

class ScriptKeys : public KeySource {
public:
    explicit ScriptKeys(std::deque<int> keys) : keys_(std::move(keys)) {}
    std::optional<int> next() override;

private:
    std::deque<int> keys_;
};

// Parses a key script: names separated by spaces or commas, each with an
// optional "*N" repeat. Names: up down left right home end pgup pgdn (the
// keypad diagonals), space enter, or a single character such as y or n.
// Throws std::invalid_argument naming the bad token.
std::deque<int> parse_key_script(const std::string& script);

class Town {
public:
    // `exe` supplies the program's own text; it must outlive the Town.
    Town(const formats::MtpMap& map, int map_number, formats::MapPosition start,
         const formats::OriginalExe& exe);

    // BTECH.EXE 1000:4cac (Ghidra FUN_1000_4cac)
    // The left panel: the party box (window 3) and the box above it (window 4).
    void draw_panel();

    // Takes keys until the source runs out. Each key is one turn.
    void run(KeySource& keys);

    formats::MapPosition position() const { return position_; }
    MapView view() const;
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
    // BTECH.EXE 24d3:17c6 (Ghidra FUN_24d3_17c6)
    void open_message_box();
    // BTECH.EXE 1000:1a13 (Ghidra FUN_1000_1a13)
    std::optional<bool> yes_no(bool yes_first, KeySource& keys);

    const formats::MtpMap& map_;
    int map_number_;
    std::string tile_sheet_;
    formats::MapPosition position_;
    const formats::OriginalExe& exe_;
    Party party_;
    TextScreen text_;
    std::vector<std::string> log_;
    bool question_open_ = false;
    std::optional<int> entered_place_;
};

}  // namespace chr::game
