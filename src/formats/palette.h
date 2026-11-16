// The 16-colour palettes the original sets for its pictures. A .CMP file holds
// colour numbers only; the colours come from tables in the program, set
// before each picture is shown (docs/formats/cmp.md, "Palette").
#pragma once

#include <array>
#include <cstdint>
#include "formats/tables.h"

namespace chr::formats {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};

using Palette16 = std::array<Rgb, 16>;

// The colour an EGA card in its 200-line 16-colour mode shows for a palette
// register value: the standard IRGB colours (bit 4 is the bright bit).
Rgb ega_200_line_colour(std::uint8_t register_value);

// The palette the program sets on the 256K EGA/VGA display (choice 3) for the
// Infocom and title screens, and puts back after screens with their own
// colours (15ab:1afe), so the map is drawn with it too.
Palette16 standard_palette_ega(const Tables& tables);

}  // namespace chr::formats
