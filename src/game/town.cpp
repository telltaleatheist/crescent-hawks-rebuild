#include "game/town.h"

#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace chr::game {

std::optional<int> ScriptKeys::next() {
    if (keys_.empty()) return std::nullopt;
    const int k = keys_.front();
    keys_.pop_front();
    return k;
}

std::deque<int> parse_key_script(const std::string& script) {
    std::deque<int> keys;
    std::string token;
    auto add = [&](const std::string& t) {
        if (t.empty()) return;
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
        for (int i = 0; i < count; ++i) keys.push_back(k);
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
           const formats::OriginalExe& exe)
    : map_(map), map_number_(map_number), tile_sheet_(tile_sheet_for_map(map_number)),
      position_(start), exe_(exe) {}

MapView Town::view() const { return view_of(map_, tile_sheet_, position_); }

void Town::draw_panel() {
    text_.fill(0, 0, 13, kTextRows, 0);  // the panel starts black
    text_.select_window(3);
    text_.frame(3);
    text_.put_text(exe_.text("text.party_heading"), 9, 13, 15, 0);
    // The first four characters of the party, a line each from row 14, two
    // rows apart; the three bars beside each name (1000:4bc1) are not drawn yet.
    int line = 0;
    for (int i = 0; i < 8 && line < 4; ++i) {
        if (!party_.present[i]) continue;
        text_.put_text(exe_.text("name." + std::to_string(i)), 1, 14 + 2 * line, 15, 0);
        ++line;
    }
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
    text_.print(exe_.get(yes ? "text.yes_picked" : "text.no_picked"));
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
        text_.print(exe_.get(yes ? "text.yes_picked" : "text.no_picked"));
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
        t.print(town_.exe_.get("text.enter_before"));
        const auto& name = town_.map_.places[std::size_t(place)];
        t.print(name);
        t.print(town_.exe_.get("text.enter_after"));
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

void Town::run(KeySource& keys) {
    while (!question_open_ && !entered_place_) {
        const auto k = keys.next();
        if (!k) break;
        turn(*k, keys);
    }
}

void Town::turn(int k, KeySource& keys) {
    // The map is drawn again every turn (1000:00bb), over the message box.
    text_.forget(13, 0, kTextColumns - 13, kTextRows);
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
                       map_.places[std::size_t(*r.offered_place)] + ")");
        return;
    }
    position_ = r.position;
    char where[32];
    std::snprintf(where, sizeof where, " at %04x,%04x", position_.x, position_.y);
    std::string line;
    if (r.offered_place)
        line += "asked to enter place " + std::to_string(*r.offered_place) +
                " (no); ";
    switch (r.outcome) {
    case StepOutcome::Moved: line += "moved"; break;
    case StepOutcome::Blocked: line += "blocked"; break;
    case StepOutcome::LeftMap: line += "left the map"; break;
    case StepOutcome::TooDeep:
        open_message_box();
        text_.print(exe_.get("text.too_deep"));
        line += "too deep";
        break;
    }
    log_.push_back(line + where);
}

}  // namespace chr::game
