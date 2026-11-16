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
