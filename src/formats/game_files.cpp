#include "formats/game_files.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace chr::formats {

namespace {
bool same_ignoring_case(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::toupper(x) == std::toupper(y);
           });
}
}  // namespace

std::optional<std::filesystem::path> find_game_file(const std::filesystem::path& dir,
                                                    std::string_view name) {
    std::error_code ec;
    const auto exact = dir / std::filesystem::path(name);
    if (std::filesystem::is_regular_file(exact, ec)) return exact;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) &&
            same_ignoring_case(entry.path().filename().string(), name))
            return entry.path();
    }
    return std::nullopt;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + path.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace chr::formats
