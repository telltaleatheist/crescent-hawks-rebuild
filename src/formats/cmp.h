// .CMP: the original's compressed full-screen pictures (docs/formats/cmp.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace chr::formats {

struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The picture in the program's own working form: 320x200, 16 colours, two
// pixels to a byte (the high nibble is the left pixel), 160 bytes to a row.
inline constexpr int kCmpWidth = 320;
inline constexpr int kCmpHeight = 200;
inline constexpr int kCmpRowBytes = kCmpWidth / 2;
inline constexpr std::size_t kCmpPackedSize = std::size_t(kCmpRowBytes) * kCmpHeight;  // 32,000

// How the run-length stream is laid over the picture (the file's first data byte).
enum class CmpOrder : std::uint8_t {
    Rows,     // type byte 1: row after row, left to right
    Columns,  // any other type byte: byte-column after byte-column, top to bottom
};

struct CmpPicture {
    CmpOrder order = CmpOrder::Columns;
    std::vector<std::uint8_t> packed;  // kCmpPackedSize bytes, row-major
    std::size_t stream_bytes_left = 0;  // body bytes after the end of the picture

    // One colour number (0..15) per pixel, row-major, kCmpWidth * kCmpHeight.
    std::vector<std::uint8_t> pixels() const;
};

// The body of a .CMP file: the bytes after the 2-byte length that the
// program's loader reads into memory (type byte first).
std::span<const std::uint8_t> cmp_body(std::span<const std::uint8_t> file);

// Unpacks the run-length stream into the 32,000-byte picture and returns how
// many stream bytes it used (in every shipped file, all of them).
std::size_t unpack_cmp_runs(std::span<const std::uint8_t> stream, CmpOrder order,
                     std::span<std::uint8_t> packed);

// A whole .CMP file, as read from disk.
CmpPicture decode_cmp(std::span<const std::uint8_t> file);

}  // namespace chr::formats
