// SHA-256, to check that a file is the version we know (FIPS 180-4).
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace chr::formats {

// Lower-case hex digest of `data`.
std::string sha256_hex(std::span<const std::uint8_t> data);

}  // namespace chr::formats
