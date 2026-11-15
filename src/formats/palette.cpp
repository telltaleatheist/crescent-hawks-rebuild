#include "formats/palette.h"

namespace chr::formats {

Rgb ega_200_line_colour(std::uint8_t value) {
    // In the 200-line modes the card takes bits 0-2 as blue, green and red at
    // two-thirds strength and bit 4 as a third added to all three. Colour 6
    // is the exception every IBM-compatible monitor makes: brown, not dark yellow.
    const bool bright = value & 0x10;
    const std::uint8_t on = 0xaa, add = bright ? 0x55 : 0x00;
    Rgb c{std::uint8_t((value & 4 ? on : 0) + add), std::uint8_t((value & 2 ? on : 0) + add),
          std::uint8_t((value & 1 ? on : 0) + add)};
    if ((value & 0x17) == 0x06) c.g = 0x55;
    return c;
}

// BTECH.EXE 273d:0525 (Ghidra FUN_273d_0525)
// Sets the 16 palette registers for the chosen display. For EGA it takes 16
// colour numbers from a table and moves 8-15 up to 16-23, where the 200-line
// mode keeps its bright colours. The standard table is at
// 37e8:0000. It is the standard order with three changes: 1 is black, and 5
// and 9 swap.
Palette16 standard_palette_ega() {
    static constexpr std::array<std::uint8_t, 16> kColourNumbers = {
        0, 0, 2, 3, 4, 9, 6, 7, 8, 1, 10, 11, 12, 13, 14, 15};
    Palette16 palette;
    for (std::size_t i = 0; i < palette.size(); ++i) {
        std::uint8_t number = kColourNumbers[i];
        if (number > 7) number += 8;
        palette[i] = ega_200_line_colour(number);
    }
    return palette;
}

}  // namespace chr::formats
