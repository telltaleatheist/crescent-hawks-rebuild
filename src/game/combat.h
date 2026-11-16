// A fight's rules: movement points, paths, the computer's plans, the round
// with its shots and damage, and the end of a round (docs/combat.md 3 and 4).
// No screen and no keys: what the fight shows goes out through Combat::Hooks,
// and the Town runs the loop around it (game/fight.cpp).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "formats/mtp.h"
#include "formats/rules.h"
#include "formats/tables.h"
#include "game/fight.h"
#include "game/figures.h"
#include "game/fixes.h"
#include "game/flags.h"
#include "game/random.h"
#include "game/records.h"

namespace chr::game {

// BTECH.EXE 1000:19f3 (Ghidra FUN_1000_19f3)
// A d6: r & 7, drawn again while above 5, plus 1.
int d6(Random& random);
// BTECH.EXE 1000:19dd (Ghidra FUN_1000_19dd)
// 2d6: two d6, the first drawn first.
int two_d6(Random& random);

// The map marks (F:d457 code, F:d497 area, F:d4d7 x, F:d517 y): 64 places,
// the oldest overwritten (docs/combat.md 3.13).
struct MapMark {
    std::uint8_t code = 0, area = 0, x = 0, y = 0;
    bool operator==(const MapMark&) const = default;
};
struct MapMarks {
    static constexpr int kCount = 64;
    std::array<MapMark, kCount> marks{};
    int next = 0;  // F:d557
    // BTECH.EXE 203b:27c9 (Ghidra FUN_203b_27c9)
    void add(int code, std::uint16_t x, std::uint16_t y);
};

// 'Mech record bytes the fight uses (docs/formats/mech.md).
namespace mech {
inline constexpr int kTonnage = 0x10;
inline constexpr int kArmour = 0x11;         // 0x11-0x1b: LA LT LL H CT RA RT RL, rear LT CT RT
inline constexpr int kStructure = 0x1c;      // 0x1c-0x23: LA LT LL H CT RA RT RL
inline constexpr int kHead = 0x1f, kCentreTorso = 0x20;
inline constexpr int kLeftArm = 0x1c, kRightArm = 0x21;
inline constexpr int kLeftActuators = 0x24;  // the left arm (high nibble) and leg (low)
inline constexpr int kRightActuators = 0x25;
inline constexpr int kEngineSinks = 0x26;
inline constexpr int kAmmunition = 0x27;     // one per weapon slot, ff: unlimited
inline constexpr int kWalk = 0x31, kJump = 0x32;
inline constexpr int kItems = 0x33, kItemsEnd = 0x56;
inline constexpr int kEngineHits = 0x75, kGyroHits = 0x76, kSensorHits = 0x77, kLifeSupport = 0x78;
inline constexpr int kPilot = 0x79, kSecond = 0x7a;
inline constexpr int kHeatSinkItem = 0x22;
}  // namespace mech

// The weapon record's bytes (DG:2ed8 + 0x11 w).
namespace weapon {
inline constexpr int kDamage = 0x0b;   // personal: low nibble + (high nibble) d6
inline constexpr int kShots = 0x0c;    // bit 7: a personal weapon; else the cluster column
inline constexpr int kHeat = 0x0d;     // low nibble
inline constexpr int kShortMedium = 0x0e;  // S in bits 5-7, M in bits 0-4
inline constexpr int kLong = 0x0f;
inline constexpr int kSkill = 0x10;
inline constexpr int kKick = 0x20, kInferno = 0x0b;
}  // namespace weapon

// The fight's tables in F: that are not the figures' places (fight.h):
// one entry per figure, or per 'Mech record.
struct CombatState {
    static constexpr int kFigures = FightFigures::kCount;
    static constexpr int kOrderBytes = 0x30;  // 12 orders of (type, area, x, y)
    static constexpr int kSlots = 12;         // weapon slots; 11 is the Kick
    static constexpr int kPathBytes = 0x18;   // 12 steps of (dx, dy); 2 ends it
    std::array<std::array<std::uint8_t, kOrderBytes>, kFigures> orders{};   // F:32c6; type ff: none
    std::array<std::array<std::uint8_t, kSlots>, kFigures> targets{};       // F:3800; ff none, bit 7 fired
    std::array<std::array<std::int8_t, kPathBytes>, kFigures> paths{};      // F:40b4
    std::array<std::uint8_t, kFigures> path_step{};      // F:0078: steps of the path taken this round
    std::array<std::int8_t, kFigures> terrain{};         // F:32ae (set by the figure drawer)
    std::array<std::uint8_t, kFigures> tile{};           // F:3750: the tile under it (the drawer)
    std::array<std::uint8_t, kFigures> standing_on{};    // F:45ce (the drawer)
    std::array<std::uint8_t, kFigures> sink{};           // F:4554 (the drawer)
    std::array<std::int8_t, kFigures> parked_facing{};   // F:45b6
    std::array<std::int8_t, 8> heat{};                   // F:006e, by 'Mech record
    std::array<std::int8_t, 8> weapon_heat{};            // F:0092: this round's
    std::array<std::uint8_t, 8> burning{};               // F:d576: rounds left
    std::array<std::uint8_t, 8> wreck_letter{};          // F:323e: a destroyed record's first byte
    bool fight_on = true;        // DG:014a: 0 when the leader is killed
    bool person_ran = false;     // F:3992: a party person was planned against a 'Mech
    bool enemy_people_ran = false;  // F:374c
    bool spectator = false;      // F:e48e: the Arena's spectator (figure 13)
};

class Combat {
public:
    // What a fight shows (and only shows): texts for 1e31:1dab (it prints
    // only with messages on, DG:2e38), the window 4 clear and the pause
    // 1e31:1dcc.
    struct Hooks {
        std::function<void(const std::string& id)> print;
        std::function<void()> clear_box;
        std::function<void()> pause;
        std::function<void(const std::string&)> log;
    };

    Combat(const formats::MtpMap& map, int blocking, formats::MapPosition& party, FightFigures& figs,
           FigureAnimation& animation, std::array<formats::MechRecord, 8>& mechs,
           std::array<CharacterRecord, 16>& characters, Flags& flags, Random& random, CombatState& state,
           MapMarks& marks, const formats::Rules& rules, const formats::Tables& tables, const Fixes& fixes,
           int messages, bool graphics, Hooks hooks);

    // BTECH.EXE 203b:22bc (Ghidra FUN_203b_22bc)
    // 'Mech record m's movement points for an order type (0 walk, 1 run,
    // 2 jump, -1 none), into F:3770.
    int mech_mp(int m, int type);
    // BTECH.EXE 203b:2474 (Ghidra FUN_203b_2474)
    // A person's movement points; n is the figure as the caller passes it.
    int person_mp(int n);
    // BTECH.EXE 203b:193b (Ghidra FUN_203b_193b)
    // Simulates figure f's orders from where it stands and writes its path.
    void simulate_path(int f);
    // BTECH.EXE 1e31:03ab (Ghidra FUN_1e31_03ab)
    // The computer plans figure f: a target, one order, every weapon at it.
    void plan(int f);
    // BTECH.EXE 203b:1482 (Ghidra FUN_203b_1482)
    // Plans the active figures among the twelve from `first` (0 or 12).
    void plan_side(int first);
    // BTECH.EXE 22e8:000c (Ghidra FUN_22e8_000c)
    // One round of 12 phases. `mission`: a fight of kinds 1-3.
    void round(bool mission);

    // BTECH.EXE 1e31:0bb5 (Ghidra FUN_1e31_0bb5)
    // The distance from the party's position.
    int distance(std::uint16_t x, std::uint16_t y) const;
    // BTECH.EXE 1e31:0f24 (Ghidra FUN_1e31_0f24)
    // The range band (0 short, 1 medium, 2 long, 3 out) from the party's
    // position to figure t with weapon w.
    int range_band(int t, int w) const;
    // BTECH.EXE 1e31:10a2 (Ghidra FUN_1e31_10a2)
    // The weapon in 'Mech record m's slot (item - 1, bit 7 if destroyed), or
    // ff when there is none or its ammunition is out.
    int slot_weapon(int m, int slot) const;
    // BTECH.EXE 1e31:1bfe (Ghidra FUN_1e31_1bfe)
    // Line of sight from the party's position (figure s) to figure t at (x, y).
    bool line_of_sight(int s, int t, std::uint16_t x, std::uint16_t y) const;
    // BTECH.EXE 1e31:16ab (Ghidra FUN_1e31_16ab)
    // Whether the party's position is taken for figure f; with `squash`, a
    // 'Mech steps on enemy people there.
    bool place_taken(int f, bool squash);

private:
    static bool is_mech(int f) { return f < 4 || (f >= 12 && f < 16); }
    static int mech_of(int f) { return f < 12 ? f : f - 8; }
    static int character_of(int f) { return f < 12 ? f - 4 : f - 8; }
    const formats::WeaponRecord& weapon_record(int w) const;
    // 3 x character c's byte 2 / 4, at least 3, halved in heavy armour.
    int foot_mp(int c) const;
    // The cell window (P:07ad) at `index` from the view's corner, around the
    // party's position.
    int window(int index) const;
    // BTECH.EXE 1e31:0006 (Ghidra FUN_1e31_0006)
    // One step of figure f toward (tx, ty) from F:e486/e488, the view place
    // (rel_x, rel_y) followed for the window.
    void step_toward(int f, std::uint16_t tx, std::uint16_t ty, int rel_x, int rel_y);
    // The firing step of one figure and slot (22e8:000c's second loop).
    // True when the slot fired (the figure's turn in this phase is over).
    bool fire(int s, int slot, bool mission, const std::array<std::int8_t, 24>& moved);
    void damage_mech(int s, int t, int w, int dir, int to_hit, bool mission, bool& destroyed);
    void damage_person(int t, int w, int to_hit, bool& killed);
    // BTECH.EXE 1e31:11ab (Ghidra FUN_1e31_11ab)
    void critical(int m, int location);
    // BTECH.EXE 1e31:163e (Ghidra FUN_1e31_163e)
    int destroy_item(int m, int first, int count);
    // BTECH.EXE 1e31:1122 (Ghidra FUN_1e31_1122)
    int transfer(int location);
    // BTECH.EXE 1d43:0a35 (Ghidra FUN_1d43_0a35)
    void destroy_mech(int f);
    // BTECH.EXE 22e8:1e46 (Ghidra FUN_22e8_1e46)
    void fire_check(int t, int dx, int dy);
    // BTECH.EXE 22e8:12c7 (Ghidra FUN_22e8_12c7), what it does without the
    // combat graphics.
    void after_shot(int t, bool killed, bool destroyed, int c);
    // BTECH.EXE 1e31:0c63 (Ghidra FUN_1e31_0c63)
    void end_round(const std::array<std::int8_t, 24>& moved);
    // BTECH.EXE 1c67:0d7e (Ghidra FUN_1c67_0d7e): every path zeroed.
    void stop_everyone();
    void print(const std::string& id) const;
    void note(const std::string& line) const;

    const formats::MtpMap& map_;
    int blocking_;  // DG:0150
    formats::MapPosition& party_;
    FightFigures& figs_;
    FigureAnimation& animation_;
    std::array<formats::MechRecord, 8>& mechs_;
    std::array<CharacterRecord, 16>& characters_;
    Flags& flags_;
    Random& random_;
    CombatState& s_;
    MapMarks& marks_;
    const formats::Rules& rules_;
    const formats::Tables& tables_;
    const Fixes& fixes_;
    int messages_;
    bool graphics_;
    Hooks hooks_;

    // Working values the original keeps in F:, between functions.
    int mp_ = 0;                 // F:3770
    bool legs_slowed_ = false;   // F:377c
    int heat_slowed_ = 0;        // F:4592
    formats::MapPosition walker_{};  // F:e486/e488: where a path being simulated has got to
    int step_dx_ = 0, step_dy_ = 0;  // F:458e/4590: the step 1e31:0006 took
    bool target_taken_ = false;  // F:d57e
    bool destroyed_ = false;     // F:e484
    bool critical_hit_ = false;  // F:4586
    bool arm_off_ = false;       // F:3986
    int kick_damage_ = 0;        // DG:3103: the Kick's damage byte
};

}  // namespace chr::game
