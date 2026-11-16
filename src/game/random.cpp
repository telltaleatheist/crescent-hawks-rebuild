#include "game/random.h"

namespace chr::game {

std::uint8_t Random::next() {
    auto& [a, b, c] = s_;
    // Bit 1 of the first byte shifts into the third byte, the third byte's
    // top bit into the second.
    const int in = (a >> 1) & 1;
    const int out_c = c >> 7;
    c = std::uint8_t(c << 1 | in);
    const int out_b = b >> 7;
    b = std::uint8_t(b << 1 | out_c);
    // (a >> 2) - a - (1 - out_b), and the result's low bit goes in at the
    // top of the first byte as it shifts right.
    const std::uint8_t t = std::uint8_t((a >> 2) - a - (out_b ^ 1));
    a = std::uint8_t((t & 1) << 7 | a >> 1);
    return std::uint8_t(a ^ b);
}

}  // namespace chr::game
