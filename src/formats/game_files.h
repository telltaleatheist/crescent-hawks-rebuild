// Finding and reading the player's own copy of the original's files.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace chr::formats {

// The file in `dir` whose name matches `name` ignoring case (the DOS names
// are upper case; a copy on Linux or macOS may not be).
std::optional<std::filesystem::path> find_game_file(const std::filesystem::path& dir,
                                                    std::string_view name);

// The whole file; throws std::runtime_error if it can't be read.
std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

}  // namespace chr::formats
