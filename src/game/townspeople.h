// The map's eight townspeople (docs/townspeople.md): each walks from one of
// the map's entrances to another, goes in, waits, comes out and walks on.
// They are figures 16-23; the people's names in the .MTP file are theirs.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "formats/mtp.h"
#include "formats/tables.h"
#include "game/figures.h"
#include "game/random.h"

namespace chr::game {

// One person's record (17 words... 0x1a bytes at F:d390 + 0x1a i).
struct Walker {
    formats::MapPosition at;      // F:4024/4056 (figure 16 + i): (0, 0) while inside
    formats::MapPosition target;  // F:d394/d396: the entrance it walks to
    std::uint8_t route = 0;       // F:d398: high nibble the last entrance, low the next
    std::uint8_t wait = 0;        // F:d399: counts down while inside; 0 = out walking
    std::int8_t heading = 0;      // F:3920 (figure 16 + i): the way it last stepped
    bool operator==(const Walker&) const = default;
};

// Where the party's characters stood when the figures were last drawn
// (F:4004/4036, figures 4-11; (0, 0) for one not drawn). A step is
// refused onto any of them.
using PartyPlaces = std::array<formats::MapPosition, 8>;

// BTECH.EXE 1e31:023c..02a6: the tile a figure stepping to `next` is
// checked against: a cell of the original's 24x24-cell window around the
// party (townspeople.cpp). `outside` is set when the index falls outside the
// window, where the rebuild reads the real cell. -1 off the map.
int window_tile(const formats::MtpMap& map, formats::MapPosition party, formats::MapPosition next,
                bool& outside);

class Townspeople {
public:
    // BTECH.EXE 1000:2da8 (Ghidra FUN_1000_2da8), its last part (a map
    // loaded for a new game). Each person i draws a random number r: it
    // starts at entrance start_shuffle[r & 7], its next entrance too, and
    // comes out next at entrance r & 7. On maps above 10 nobody is out.
    // Draws 8 random numbers.
    // Its tables come from the imported tables, which must outlive it.
    static Townspeople place(const formats::MtpMap& map, int map_number, Random& random,
                             const formats::Tables& tables);

    // BTECH.EXE 1000:24c2 (Ghidra FUN_1000_24c2)
    // Every third game tick. A person inside counts down, and comes out at
    // its last entrance when the count runs out. A person out walking takes
    // a step toward its next entrance, but only while within 16 half-cells
    // or so of the party's 16x16 square; on arriving it goes in for a
    // random 0-31 rounds and picks the entrance after. `near` gets whether
    // each one is within that square (F:42f6 + i, the bytes the fight's
    // drawer later uses for figures 0-7).
    void move(const formats::MtpMap& map, formats::MapPosition party, const PartyPlaces& party_places,
              FigureAnimation& animation, Random& random, std::span<bool, 8> near);

    // BTECH.EXE 1000:051b (Ghidra FUN_1000_051b), the townspeople's part.
    // The people out walking that are in view, in order.
    std::vector<Figure> figures(const formats::MtpMap& map, formats::MapPosition party,
                                const FigureAnimation& animation) const;

    // BTECH.EXE 17dc:0d49, its first part: before a fight each person's
    // place is kept in its record (F:d390 + 0x1a i, +0/+2) and its figure
    // emptied (x and y ffff), so nobody is drawn or moves until the town
    // comes back.
    void leave_for_fight();
    // BTECH.EXE 203b:141a..1473: after the fight, back where they were.
    // Their headings are figures 16-23's in the fight (one table, F:3920 +
    // 16 + i, in the original), so the fight's come back with them.
    void back_from_fight(const std::array<std::int8_t, 8>& headings) {
        for (std::size_t i = 0; i < walkers_.size(); ++i) {
            walkers_[i].at = kept_[i];
            walkers_[i].heading = headings[i];
        }
    }

    const std::array<Walker, 8>& walkers() const { return walkers_; }
    std::array<Walker, 8>& walkers() { return walkers_; }

    // Original bug, kept by default: a person sent inside for 0 rounds stays
    // inside for good (its count is never counted down from 0, and (0, 0) is
    // never near the party). With the fix it comes out on the next round.
    bool fix_lost_walker = false;

    // F:d57e: set when a person found another figure on its target
    // (cleared at 2000:1e3b).
    bool target_taken = false;

    // Set when a person's step looked outside the original's cell window,
    // where the original reads memory the rebuild does not hold; the rebuild
    // read the real map cell instead (the caller logs it).
    bool outside_window = false;

private:
    void step(int i, const formats::MtpMap& map, formats::MapPosition party, const PartyPlaces& party_places);

    const formats::Tables* tables_ = nullptr;
    std::array<Walker, 8> walkers_{};
    std::array<formats::MapPosition, 8> kept_{};  // F:d390/d392 + 0x1a i, during a fight
    int blocking_ = 0x55;  // DGROUP:0150
    // BTECH.EXE 1e31:0006's wander pattern: every 30 steps (DGROUP:315a)
    // the order of the directions tried after the first flips (315c).
    int wobble_count_ = 100;  // its value at the start of a new game
    int wobble_ = 0;
};

// BTECH.EXE 287f:0971 (Ghidra FUN_287f_0971)
// The direction (0-7, north first, clockwise) from `from` toward `to`, the
// nearest of the eight to the line between them, or -1 when they are the
// same place.
int direction_toward(formats::MapPosition from, formats::MapPosition to, const formats::Tables& tables);

}  // namespace chr::game
