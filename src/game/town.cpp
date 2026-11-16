#include "game/town.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>

#include "game/tile_animation.h"

namespace chr::game {

std::optional<ScriptItem> ScriptKeys::next_item() {
    if (items_.empty()) return std::nullopt;
    const auto item = items_.front();
    items_.pop_front();
    return item;
}

std::optional<int> ScriptKeys::next() {
    while (!items_.empty() && items_.front().idle > 0) items_.pop_front();
    const auto item = next_item();
    if (!item) return std::nullopt;
    return item->key;
}

std::deque<ScriptItem> parse_key_script(const std::string& script) {
    std::deque<ScriptItem> keys;
    std::string token;
    auto add = [&](const std::string& t) {
        if (t.empty()) return;
        if (t.rfind("idle:", 0) == 0) {
            int n = 0;
            try {
                n = std::stoi(t.substr(5));
            } catch (const std::exception&) {
                throw std::invalid_argument("bad count in \"" + t + "\"");
            }
            if (n < 0) throw std::invalid_argument("bad count in \"" + t + "\"");
            if (n > 0) keys.push_back({0, n});
            return;
        }
        std::string name = t;
        int count = 1;
        if (const auto star = t.find('*'); star != std::string::npos) {
            name = t.substr(0, star);
            try {
                count = std::stoi(t.substr(star + 1));
            } catch (const std::exception&) {
                throw std::invalid_argument("bad repeat in \"" + t + "\"");
            }
        }
        int k = 0;
        if (name == "up") k = key::kUp;
        else if (name == "down") k = key::kDown;
        else if (name == "left") k = key::kLeft;
        else if (name == "right") k = key::kRight;
        else if (name == "home") k = key::kHome;
        else if (name == "end") k = key::kEnd;
        else if (name == "pgup") k = key::kPageUp;
        else if (name == "pgdn") k = key::kPageDown;
        else if (name == "space") k = key::kSpace;
        else if (name == "enter") k = key::kEnter;
        else if (name.size() == 1) k = static_cast<unsigned char>(name[0]);
        else throw std::invalid_argument("unknown key \"" + name + "\"");
        for (int i = 0; i < count; ++i) keys.push_back({k, 0});
    };
    for (const char c : script) {
        if (c == ' ' || c == ',') {
            add(token);
            token.clear();
        } else {
            token += c;
        }
    }
    add(token);
    return keys;
}

Town::Town(const formats::MtpMap& map, int map_number, formats::MapPosition start,
           const TextProvider& texts)
    : map_(map), map_number_(map_number), tile_sheet_(tile_sheet_for_map(map_number)),
      position_(start), texts_(texts) {}

MapView Town::view() const {
    auto view = view_of(map_, tile_sheet_, position_);
    if (tiles_animate(map_number_))
        for (auto& cell : view.cells)
            if (const auto frame = animated_tile(std::uint8_t(cell.tile.index), phase_)) cell.tile = *frame;
    return view;
}

std::vector<Figure> Town::figures() const { return party_figures(map_, position_, party_, animation_); }

// BTECH.EXE 1000:4aa6 (Ghidra FUN_1000_4aa6)
// Beside each of the first four characters' names (row 14, 16, ...): bars at
// columns 9, 10 and 11. The first bar's top part, its number less a tenth
// of byte 15 (at least 1), is drawn as a warning.
std::vector<StatBar> Town::bars() const {
    std::vector<StatBar> bars;
    int line = 0;
    for (int i = 0; i < 8 && line < 4; ++i) {
        if (!party_.present[i]) continue;
        const auto& s = stats_[i];
        const int row = 14 + 2 * line;
        bars.push_back({9, row, s.b, s.b - std::max(s.byte15 / 10, 1)});
        bars.push_back({10, row, s.d, 0});
        bars.push_back({11, row, s.c, 0});
        ++line;
    }
    return bars;
}

void Town::draw_panel() {
    text_.fill(0, 0, 13, kTextRows, 0);  // the panel starts black
    text_.select_window(3);
    text_.frame(3);
    text_.put_text(lookup(texts_, "panel.party_heading"), 9, 13, 15, 0);
    // The first four characters of the party, a line each from row 14, two
    // rows apart, with their bars (bars()).
    int line = 0;
    for (int i = 0; i < 8 && line < 4; ++i) {
        if (!party_.present[i]) continue;
        text_.put_text(lookup(texts_, "character." + std::to_string(i)), 1, 14 + 2 * line, 15, 0);
        ++line;
    }
    draw_cbills();
    text_.select_window(4);
    text_.frame(4);
}

void Town::open_message_box() {
    text_.select_window(7);
    text_.clear_window();
    text_.frame(0);
}

std::optional<bool> Town::yes_no(bool yes_first, KeySource& keys) {
    const int saved_fg = text_.window().fg;
    bool yes = yes_first;
    text_.print(lookup(texts_, yes ? "ui.answer_yes_picked" : "ui.answer_no_picked"));
    for (;;) {
        const auto k = keys.next();
        if (!k) return std::nullopt;
        bool done = false;
        const int up = std::toupper(*k);
        if (*k == key::kSpace || *k == key::kEnter) done = true;
        else if (up == 'Y') yes = done = true;
        else if (up == 'N') { yes = false; done = true; }
        else if (*k == key::kLeft) yes = true;
        else if (*k == key::kRight) yes = false;
        // Every key, even one it ignores, puts the answer line up again.
        text_.cursor_up();
        text_.print(lookup(texts_, yes ? "ui.answer_yes_picked" : "ui.answer_no_picked"));
        if (done) break;
    }
    text_.set_fg(saved_fg);
    return yes;
}

class Town::Question : public EntranceQuestion {
public:
    Question(Town& town, KeySource& keys) : town_(town), keys_(keys) {}
    bool ask(int place) override {
        auto& t = town_.text_;
        town_.open_message_box();
        t.print(lookup(town_.texts_, "walk.enter_question"));
        t.print(lookup(town_.texts_, place_text_id(town_.map_number_, place)));
        t.print(lookup(town_.texts_, "walk.enter_question_end"));
        const auto answer = town_.yes_no(true, keys_);
        if (!answer) {
            town_.question_open_ = true;
            return false;
        }
        return *answer;
    }

private:
    Town& town_;
    KeySource& keys_;
};

void Town::draw_cbills() {
    const int previous = text_.window_number();
    text_.select_window(3);
    text_.set_cursor(0, 9);
    text_.print(lookup(texts_, "panel.cbills_label"));
    // BTECH.EXE 1000:28a2 (Ghidra FUN_1000_28a2): light green (CGA: colour 1).
    text_.set_fg(10);
    // The amount in decimal, padded with spaces to ten characters so a
    // shorter number covers a longer one.
    std::string amount = std::to_string(money_.cbills);
    if (amount.size() < 10) amount.resize(10, ' ');
    text_.print(amount);
    text_.set_fg(15);
    text_.select_window(previous);
}

void Town::run(ScriptKeys& input) {
    while (!question_open_ && !entered_place_) {
        const auto item = input.next_item();
        if (!item) break;
        if (item->idle > 0) {
            for (int i = 0; i < item->idle; ++i) idle_pass();
        } else {
            key(item->key, input);
        }
    }
}

void Town::idle_pass() {
    if (question_open_ || entered_place_) return;
    if (--countdown_ < 0) game_tick();
}

void Town::key(int k, KeySource& more) {
    if (question_open_ || entered_place_) return;
    turn(k, more);
    if (question_open_ || entered_place_) return;
    // BTECH.EXE 1000:00fb: the figures' walking frames move on, then the key
    // half goes straight on to a game tick.
    animation_.key(k);
    game_tick();
}

void Town::game_tick() {
    // Timers that count down every tick when set (DGROUP:d335, d343-d345,
    // d329, d320-d322): none is set in the town at the start of a game; their
    // uses are not read yet.
    // A random number is drawn every tick for the chance of an encounter
    // (DGROUP:d330 masks it, 203b:000a starts the fight). Only outdoors,
    // which is not rebuilt yet; the draw keeps the sequence in step.
    const std::uint8_t r = random_.next();
    (void)r;  // outdoors, (r & DGROUP:d330) == 0 starts a fight: not rebuilt
    if (money_tick(money_, outdoors_, random_)) draw_cbills();
    countdown_ = 10;
    if (tiles_animate(map_number_)) phase_ = (phase_ + 1) % 3;  // 1000:240b
    // BTECH.EXE 1000:24c2 (Ghidra FUN_1000_24c2) moves the townspeople on
    // every third tick: not rebuilt yet.
    if (third_++ == 2) third_ = 0;
    // The map and the figures are drawn again (DGROUP:014a is set in town),
    // over any message box (1000:046e).
    text_.forget(13, 0, kTextColumns - 13, kTextRows);
    ++ticks_;
}

namespace {

// The name of a step's direction, in the order of the keys at DGROUP:0160.
std::string direction_text_id(Step s) {
    static constexpr const char* kNames[3][3] = {
        {"northwest", "north", "northeast"}, {"west", "", "east"}, {"southwest", "south", "southeast"}};
    return std::string("walk.direction.") + kNames[s.dy + 1][s.dx + 1];
}

}  // namespace

void Town::turn(int k, KeySource& keys) {
    const auto step = step_for_key(k);
    if (!step) {
        log_.push_back("key " + std::to_string(k) + ": nothing");
        return;
    }
    Question question(*this, keys);
    const auto r = take_step(map_, map_number_, position_, party_, *step, question);
    if (question_open_) {  // the original waits for the answer before anything else
        log_.push_back("asked to enter place " + std::to_string(*r.offered_place) + " (waiting)");
        return;
    }
    if (r.entered) {
        // BTECH.EXE 17dc:0008 (Ghidra FUN_17dc_0008) runs the place; not rebuilt
        // yet, so the walk stops here.
        entered_place_ = *r.offered_place;
        log_.push_back("entered place " + std::to_string(*r.offered_place) + " (" +
                       lookup(texts_, place_text_id(map_number_, *r.offered_place)) + ")");
        return;
    }
    position_ = r.position;
    char where[32];
    std::snprintf(where, sizeof where, " at %04x,%04x", position_.x, position_.y);
    std::string line;
    if (r.offered_place)
        line += "asked to enter place " + std::to_string(*r.offered_place) +
                " (no); ";
    // The main loop then writes the direction in the upper panel box, unless
    // a message or question came up this turn (DGROUP:d55c). Blocked steps
    // are logged too.
    if (!r.offered_place && r.outcome != StepOutcome::TooDeep) {
        text_.select_window(4);
        text_.set_fg(15);
        text_.print(lookup(texts_, direction_text_id(*step)));
    }
    switch (r.outcome) {
    case StepOutcome::Moved: line += "moved"; break;
    case StepOutcome::Blocked: line += "blocked"; break;
    case StepOutcome::LeftMap: line += "left the map"; break;
    case StepOutcome::TooDeep:
        open_message_box();
        text_.print(lookup(texts_, "walk.too_deep"));
        line += "too deep";
        break;
    }
    log_.push_back(line + where);
}

}  // namespace chr::game
