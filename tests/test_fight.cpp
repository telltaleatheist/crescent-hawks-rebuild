#include "game/fight.h"
#include "test.h"

using namespace chr::game;

TEST(mech_sink_by_tile_and_quarters) {
    CHECK(mech_sink(-1, 1) == 0);    // off the map
    CHECK(mech_sink(0x00, 1) == 0);  // no water quarter
    CHECK(mech_sink(0x01, 1) == 8);  // shallow water on the quarter tested
    CHECK(mech_sink(0x01, 4) == 0);  // another quarter
    CHECK(mech_sink(0x0f, 4) == 16); // all four quarters, group 0: deep
    CHECK(mech_sink(0x2f, 1) == 16);
    CHECK(mech_sink(0x1f, 1) == 8);  // group 1 never sinks deep
    CHECK(mech_sink(0x31, 1) == 0);  // group 3 and above: not water
    CHECK(mech_sink(0xf7, 1) == 0);  // 0xf6 and above never sink
}

#include "game/combat.h"

namespace {

// A Combat on empty tables, for the rules that read only records.
struct Bench {
    chr::formats::Tables tables;
    chr::formats::MtpMap map;
    chr::formats::MapPosition party{0x0c40, 0xc040};
    FightFigures figs;
    FigureAnimation animation{tables};
    std::array<chr::formats::MechRecord, 8> mechs{};
    std::array<CharacterRecord, 16> characters{};
    Flags flags;
    Random random;
    CombatState state;
    MapMarks marks;
    chr::formats::Rules rules;
    Fixes fixes;
    Terrain terrain{tables, fixes};
    Combat combat{terrain, 0x55, party, figs, animation, mechs, characters, flags, random, state, marks,
                  rules, tables, fixes, 0, false, {}};
};

}  // namespace

TEST(d6_draws_again_above_five) {
    Random r;
    for (int i = 0; i < 500; ++i) {
        const int v = d6(r);
        CHECK(v >= 1 && v <= 6);
    }
    for (int i = 0; i < 500; ++i) {
        const int v = two_d6(r);
        CHECK(v >= 2 && v <= 12);
    }
}

TEST(mech_mp_legs_heat_and_running) {
    Bench b;
    auto& m = b.mechs[0];
    m[mech::kWalk] = 5;
    m[mech::kJump] = 3;
    m[mech::kLeftActuators] = m[mech::kRightActuators] = 0xff;
    CHECK(b.combat.mech_mp(0, 0) == 5);
    CHECK(b.combat.mech_mp(0, 1) == 8);  // 5 + 2, +1: the walking MP is odd
    CHECK(b.combat.mech_mp(0, 2) == 3);  // a jump: no heat penalty
    b.state.heat[0] = 9;
    CHECK(b.combat.mech_mp(0, 0) == 4);  // 9 / 5
    CHECK(b.combat.mech_mp(0, 2) == 3);
    b.state.heat[0] = 30;
    CHECK(b.combat.mech_mp(0, 2) == 0);  // shut down
    b.state.heat[0] = 0;
    m[mech::kLeftActuators] = 0xf7;      // one hip gone: half, rounded up
    CHECK(b.combat.mech_mp(0, 0) == 3);
    m[mech::kLeftActuators] = m[mech::kRightActuators] = 0xf0;  // no hip: 1
    CHECK(b.combat.mech_mp(0, 0) == 1);
}

TEST(distance_is_max_half_plus_min) {
    Bench b;
    // Bug 1: max / 2 + min, not max + min / 2.
    CHECK(b.combat.distance(0x0c40 + 8, 0xc040) == 4);
    CHECK(b.combat.distance(0x0c40 + 8, 0xc040 + 2) == 6);
    // The next area along x continues the same line.
    CHECK(b.combat.distance(0x0d00, 0xc040) == (0x80 - 0x40) / 2);
    b.fixes.fix_distance = true;
    CHECK(b.combat.distance(0x0c40 + 8, 0xc040 + 2) == 9);
}
