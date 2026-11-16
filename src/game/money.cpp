#include "game/money.h"

namespace chr::game {

namespace {

// DGROUP:02a2 and 02a8: per investment, the bits of a random number that
// must all be clear for a loss, and the rate.
constexpr std::array<int, 3> kLossMask = {0xff, 0x07, 0x01};
constexpr std::array<int, 3> kRate = {96, 90, 48};

// The run-time library's 32-bit multiply and divide (287f:3e2e, 287f:3e62):
// the product wraps to 32 bits, the quotient rounds toward zero.
std::int32_t mul32(std::int32_t a, std::int32_t b) {
    return std::int32_t(std::uint32_t(std::int64_t(a) * b));
}

}  // namespace

bool money_tick(Money& money, bool outdoors, Random& random) {
    if (money.countdown-- != 0) return false;

    const std::int32_t total = std::int32_t(std::uint32_t(money.invested[0]) + std::uint32_t(money.invested[1]) +
                                            std::uint32_t(money.invested[2]) + std::uint32_t(money.cbills));
    const bool short_of_money = std::int32_t(money.floor) > total;
    bool redraw = false;
    if (!outdoors) {
        if (short_of_money) money.cbills = std::int32_t(std::uint32_t(money.cbills) + 15);
        redraw = true;  // 1e31:1fdf
    }
    // The investments move only outdoors or while short of money (the total
    // from before the 15 were added).
    if (outdoors || short_of_money) {
        for (int i = 0; i < 3; ++i) {
            auto& v = money.invested[i];
            if ((random.next() & kLossMask[i]) == 0) {
                v = mul32(v, kRate[i]) / 110;
            } else {
                v = mul32(v, 100) / kRate[i];
                // The third is cut to a quarter once it is over 18,000
                // (a logical shift of all 32 bits).
                if (i == 2 && std::uint32_t(v) > 18000) v = std::int32_t(std::uint32_t(v) >> 2);
            }
        }
    }
    return redraw;
}

}  // namespace chr::game
