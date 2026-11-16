#include "formats/anm.h"

#include "formats/cmp.h"  // FormatError
#include "formats/game_files.h"

namespace chr::formats {

namespace {

constexpr int kSpeedAt = 0x32;
constexpr int kDataAt = 0x33;
// A frame-list entry s names its delay byte at file offset s - 0x21: the
// program keeps the delays 0x21 bytes before the file in memory
// (DGROUP:42a2, the file at 42c3).
constexpr int kDelayBase = 0x21;

std::uint8_t byte_at(const std::vector<std::uint8_t>& f, std::size_t at) {
    if (at >= f.size()) throw FormatError("an .ANM file ends inside a frame");
    return f[at];
}

// BTECH.EXE 287f:23ec (Ghidra FUN_287f_23ec)
// XORs one frame's changes onto `packed` (two pixels a byte, the left one in
// the high nibble) and returns where the next frame starts. The changes are
// runs: n < 0x80 is n bytes as they are; n >= 0x80 is the next byte 256 - n
// times; 00 hh ll v is v (hh ll) times. A frame ends after 3872 bytes,
// wherever that falls in a run.
std::size_t apply_frame(const std::vector<std::uint8_t>& f, std::size_t at, std::vector<std::uint8_t>& packed) {
    int out = 0;
    while (true) {
        int count = byte_at(f, at);
        bool run = false;
        if (count == 0) {
            count = byte_at(f, at + 1) << 8 | byte_at(f, at + 2);
            at += 2;
            run = true;
        } else if (count >= 0x80) {
            count = 0x100 - count;
            run = true;
        }
        // The program counts down a 16-bit loop: a run of 0 goes round 65536 times.
        long n = count == 0 ? 0x10000 : count;
        for (; n > 0; --n) {
            const std::uint8_t v = run ? byte_at(f, at + 1) : byte_at(f, ++at);
            packed[out++] ^= v;
            if (out == kAnmFrameBytes) return at + 1 + (run ? 1 : 0);
        }
        at += run ? 2 : 1;
    }
}

}  // namespace

AnmPicture decode_anm(const std::vector<std::uint8_t>& file) {
    if (file.size() < kDataAt) throw FormatError("an .ANM file shorter than its header");
    AnmPicture p;
    p.speed = static_cast<std::int8_t>(file[kSpeedAt]);
    // The first frame always shows; after frame k, the next one shows while
    // entry k + 1 is not 0.
    p.sequence.push_back(file[0]);
    while (p.sequence.size() < kDataAt && file[p.sequence.size()] != 0) p.sequence.push_back(file[p.sequence.size()]);
    std::vector<std::uint8_t> packed(kAnmFrameBytes, 0);
    std::size_t at = kDataAt;
    const std::size_t count = p.sequence.size();
    for (std::size_t k = 0; k < count; ++k) {
        at = apply_frame(file, at, packed);
        std::vector<std::uint8_t> pixels(kAnmSize * kAnmSize);
        for (int i = 0; i < kAnmFrameBytes; ++i) {
            pixels[2 * i] = packed[i] >> 4;
            pixels[2 * i + 1] = packed[i] & 0x0f;
        }
        p.frames.push_back(std::move(pixels));
        const int s = k < p.sequence.size() ? p.sequence[k] : 0;
        const int delay = s >= kDelayBase && s - kDelayBase < static_cast<int>(file.size())
                              ? static_cast<std::int8_t>(file[s - kDelayBase])
                              : 0;
        p.delays.push_back(delay * p.speed * 3 >> 2);
    }
    return p;
}

AnmPicture read_anm(const std::filesystem::path& path) { return decode_anm(read_file(path)); }

}  // namespace chr::formats
