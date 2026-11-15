#include "formats/cmp.h"

namespace chr::formats {

// BTECH.EXE 273d:063b (Ghidra FUN_273d_063b)
// The loader opens the file, reads a 2-byte length and then exactly that many
// bytes into its picture buffer. (On failure it asks for the disk and tries
// again; the rebuild reports an error instead.)
std::span<const std::uint8_t> cmp_body(std::span<const std::uint8_t> file) {
    if (file.size() < 3) throw FormatError("CMP: file too short");
    const std::size_t length = std::size_t(file[0]) | (std::size_t(file[1]) << 8);
    if (length < 1 || file.size() - 2 < length)
        throw FormatError("CMP: length field " + std::to_string(length) +
                          " runs past the end of the file");
    return file.subspan(2, length);
}

// BTECH.EXE 287f:22f8 (Ghidra FUN_287f_22f8), rows
// BTECH.EXE 287f:2368 (Ghidra FUN_287f_2368), columns
// The two unpackers share one run-length code and differ only in where each
// output byte lands. Each code byte, read as signed:
//   n > 0   the next n bytes are copied as they are;
//   n < 0   the next byte is repeated -n times;
//   n == 0  a 2-byte little-endian count follows, then the byte to repeat.
// Unpacking stops the moment 32,000 bytes are out, even mid-run. A zero
// 16-bit count repeats until the picture is full (the original's counter
// wraps round to 65,536).
std::size_t unpack_cmp_runs(std::span<const std::uint8_t> stream, CmpOrder order,
                     std::span<std::uint8_t> packed) {
    if (packed.size() != kCmpPackedSize) throw FormatError("CMP: output must be 32,000 bytes");

    std::size_t in = 0;
    std::size_t written = 0;
    int row = 0, column = 0;  // only for CmpOrder::Columns

    auto next = [&]() -> std::uint8_t {
        if (in >= stream.size()) throw FormatError("CMP: data ends before the picture is full");
        return stream[in++];
    };
    auto put = [&](std::uint8_t value) {
        if (order == CmpOrder::Rows) {
            packed[written] = value;
        } else {
            packed[std::size_t(row) * kCmpRowBytes + column] = value;
            if (++row == kCmpHeight) {
                row = 0;
                ++column;
            }
        }
        ++written;
    };

    while (written < kCmpPackedSize) {
        const auto code = static_cast<std::int8_t>(next());
        if (code > 0) {
            for (int i = 0; i < code && written < kCmpPackedSize; ++i) put(next());
            continue;
        }
        std::size_t count;
        if (code < 0) {
            count = std::size_t(-int(code));
        } else {
            const std::size_t lo = next();
            count = lo | (std::size_t(next()) << 8);
            if (count == 0) count = 0x10000;
        }
        const std::uint8_t value = next();
        for (std::size_t i = 0; i < count && written < kCmpPackedSize; ++i) put(value);
    }
    return in;
}

// BTECH.EXE 273d:049d (Ghidra FUN_273d_049d)
// Picks the unpacker by the body's first byte: 1 for row order, anything else
// for column order. (For the CGA display it then also reduces the picture to
// four colours; see docs/formats/cmp.md. The rebuild keeps all 16.)
CmpPicture decode_cmp(std::span<const std::uint8_t> file) {
    const auto body = cmp_body(file);
    CmpPicture picture;
    picture.order = body[0] == 1 ? CmpOrder::Rows : CmpOrder::Columns;
    picture.packed.assign(kCmpPackedSize, 0);
    const auto stream = body.subspan(1);
    picture.stream_bytes_left =
        stream.size() - unpack_cmp_runs(stream, picture.order, picture.packed);
    return picture;
}

// Every display driver reads the packed byte high nibble first: the EGA
// plane writer (287f:0260) puts the high nibble in the left pixel's bit, and
// the MCGA copy (287f:200e) writes it as the left of the two pixels.
std::vector<std::uint8_t> CmpPicture::pixels() const {
    std::vector<std::uint8_t> out(std::size_t(kCmpWidth) * kCmpHeight);
    for (std::size_t i = 0; i < packed.size(); ++i) {
        out[2 * i] = packed[i] >> 4;
        out[2 * i + 1] = packed[i] & 0x0f;
    }
    return out;
}

}  // namespace chr::formats
