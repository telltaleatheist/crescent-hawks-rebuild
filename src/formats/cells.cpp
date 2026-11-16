#include "formats/cells.h"

namespace chr::formats {

std::array<std::uint8_t, 64> picture_cell(const CmpPicture& picture, int n) {
    constexpr int kAcross = kCmpWidth / kCellSize;  // 40
    constexpr int kDown = kCmpHeight / kCellSize;   // 25
    if (n < 0 || n >= kAcross * kDown) throw FormatError("picture: no such cell");
    const int left = (n % kAcross) * kCellSize, top = (n / kAcross) * kCellSize;
    const auto pixels = picture.pixels();
    std::array<std::uint8_t, 64> out{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) out[y * 8 + x] = pixels[std::size_t(top + y) * kCmpWidth + left + x];
    return out;
}

}  // namespace chr::formats
