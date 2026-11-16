#include "formats/exepack.h"

#include <algorithm>
#include <string_view>

#include "formats/cmp.h"  // FormatError

namespace chr::formats {

namespace {

std::uint16_t u16(std::span<const std::uint8_t> d, std::size_t at) {
    if (at + 2 > d.size()) throw FormatError("EXEPACK: file is cut short");
    return std::uint16_t(d[at] | d[at + 1] << 8);
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(std::uint8_t(v));
    out.push_back(std::uint8_t(v >> 8));
}

}  // namespace

std::vector<std::uint8_t> unexepack(std::span<const std::uint8_t> data) {
    if (data.size() < 28 || data[0] != 'M' || data[1] != 'Z') throw FormatError("not an MZ program");
    const auto last = u16(data, 2), pages = u16(data, 4), nrel = u16(data, 6),
               hdr_par = u16(data, 8), min_al = u16(data, 10), max_al = u16(data, 12),
               ip = u16(data, 20), cs = u16(data, 22);
    if (nrel != 0) throw FormatError("the program has relocations: not EXEPACKed");
    const std::size_t size = last ? std::size_t(pages - 1) * 512 + last : std::size_t(pages) * 512;
    const std::size_t hdr = std::size_t(hdr_par) * 16;
    if (size > data.size() || hdr > size) throw FormatError("EXEPACK: file is cut short");
    const auto image = data.subspan(hdr, size - hdr);

    // The EXEPACK header at the packed entry point.
    const std::size_t blk = std::size_t(cs) * 16;
    std::size_t sig_at = 0, skip = 1;
    if (ip == 0x10) {
        sig_at = blk + 14;
    } else if (ip == 0x12) {
        sig_at = blk + 16;
        skip = u16(image, blk + 14);
    } else {
        throw FormatError("EXEPACK: unexpected entry point");
    }
    if (sig_at + 2 > image.size() || image[sig_at] != 'R' || image[sig_at + 1] != 'B')
        throw FormatError("EXEPACK signature not found");
    const auto real_ip = u16(image, blk), real_cs = u16(image, blk + 2),
               exepack_size = u16(image, blk + 6), real_sp = u16(image, blk + 8),
               real_ss = u16(image, blk + 10), dest_len = u16(image, blk + 12);

    // Unpack in place, from the end backwards.
    const std::size_t dest = std::size_t(dest_len) * 16;
    std::vector<std::uint8_t> buf(image.begin(), image.begin() + std::ptrdiff_t(blk));
    if (buf.size() < dest) buf.resize(dest, 0);
    std::size_t src = blk - (skip - 1) * 16;
    while (src > 0 && buf[src - 1] == 0xff) --src;
    std::size_t dst = dest;
    for (;;) {
        if (src < 3) throw FormatError("EXEPACK: stream runs off the start");
        const std::uint8_t cmd = buf[src - 1];
        const std::size_t length = buf[src - 3] | buf[src - 2] << 8;
        src -= 3;
        if (length > dst) throw FormatError("EXEPACK: run runs off the start");
        if ((cmd & 0xfe) == 0xb0) {
            const std::uint8_t fill = buf[src - 1];
            --src;
            std::fill(buf.begin() + std::ptrdiff_t(dst - length), buf.begin() + std::ptrdiff_t(dst), fill);
        } else if ((cmd & 0xfe) == 0xb2) {
            if (length > src) throw FormatError("EXEPACK: copy runs off the start");
            std::copy_backward(buf.begin() + std::ptrdiff_t(src - length), buf.begin() + std::ptrdiff_t(src),
                               buf.begin() + std::ptrdiff_t(dst));
            src -= length;
        } else {
            throw FormatError("EXEPACK: bad command");
        }
        dst -= length;
        if (cmd & 1) break;
    }
    buf.resize(dest);

    // The relocation table follows the stub's error message.
    if (blk + exepack_size > image.size()) throw FormatError("EXEPACK: stub is cut short");
    const auto stub = image.subspan(blk, exepack_size);
    constexpr std::string_view kMessage = "Packed file is corrupt";
    const auto found = std::search(stub.begin(), stub.end(), kMessage.begin(), kMessage.end());
    if (found == stub.end()) throw FormatError("EXEPACK: relocation table not found");
    std::size_t p = std::size_t(found - stub.begin()) + kMessage.size();
    std::vector<std::pair<std::uint16_t, std::uint16_t>> relocs;  // offset, segment
    for (int seg = 0; seg < 16; ++seg) {
        const auto count = u16(stub, p);
        p += 2;
        for (int i = 0; i < count; ++i, p += 2) relocs.emplace_back(u16(stub, p), std::uint16_t(seg * 0x1000));
    }

    // A plain MZ header, the same fields tools/unexepack.py writes.
    constexpr std::size_t kRelOff = 0x1c;
    std::size_t hdr_len = kRelOff + 4 * relocs.size();
    hdr_len = (hdr_len + 511) / 512 * 512;
    const std::size_t total = hdr_len + buf.size();
    const long need_par = long(size - hdr) / 16 + min_al - dest_len;
    const long stack_par = (long(real_ss) * 16 + real_sp + 15) / 16 - dest_len;
    const long new_min = std::max({0L, need_par, stack_par});

    std::vector<std::uint8_t> out{'M', 'Z'};
    for (const std::size_t v :
         {total % 512, (total + 511) / 512, relocs.size(), hdr_len / 16,
          std::size_t(std::min(new_min, 0xffffL)), std::size_t(max_al), std::size_t(real_ss),
          std::size_t(real_sp), std::size_t(0), std::size_t(real_ip), std::size_t(real_cs), kRelOff,
          std::size_t(0)})
        put16(out, std::uint16_t(v));
    for (const auto& [off, seg] : relocs) {
        put16(out, off);
        put16(out, seg);
    }
    out.resize(hdr_len, 0);
    out.insert(out.end(), buf.begin(), buf.end());
    return out;
}

}  // namespace chr::formats
